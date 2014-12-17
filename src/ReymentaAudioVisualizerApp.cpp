/*
                
        Basic Spout sender for Cinder

        Search for "SPOUT" to see what is required
        Uses the Spout dll

        Based on the RotatingBox CINDER example without much modification
        Nothing fancy about this, just the basics.

        Search for "SPOUT" to see what is required

    ==========================================================================
    Copyright (C) 2014 Lynn Jarvis.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    ==========================================================================

*/
/*
Copyright (c) 2014, Paul Houx - All rights reserved.
This code is intended for use with the Cinder C++ library: http://libcinder.org

Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and
the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/Vbo.h"
#include "cinder/Camera.h"
#include "cinder/Channel.h"
#include "cinder/ImageIo.h"
#include "cinder/MayaCamUI.h"
#include "cinder/Rand.h"
#include "cinder/audio/MonitorNode.h"
#include "cinder/audio/Device.h"
#include "cinder/gl/Light.h"
#include "Resources.h"
#include "cinder/Perlin.h"
#include "cinder/params/Params.h"

// spout
#include "spout.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class ReymentaAudioVisualizerApp : public AppNative {
public:
    void prepareSettings(Settings *settings);
	void setup();
	void update();
	void draw();
	void resize();
	void shutdown();
	void mouseDown(MouseEvent event);
	void mouseDrag(MouseEvent event);
	void mouseUp(MouseEvent event);
	void mouseWheel(MouseEvent event);
	void fileDrop(FileDropEvent event);
	void loadWaveFile(string aFilePath);
	void keyDown(KeyEvent event);

	bool				signalChannelEnd;

private:
    // -------- SPOUT -------------
    SpoutSender spoutsender;                    // Create a Spout sender object
    bool bInitialized;                          // true if a sender initializes OK
    bool bMemoryMode;                           // tells us if texture share compatible
    unsigned int g_Width, g_Height;             // size of the texture being sent out
    char SenderName[256];                       // sender name 
    gl::TextureRef spoutTexture;                   // Local Cinder texture used for sharing
    // ----------------------------
	// width and height of our mesh
	static const int				kWidth = 256;
	static const int				kHeight = 256;

	// number of frequency bands of our spectrum
	static const int				kBands = 1024;
	static const int				kHistory = 128;

	Channel32f						mChannelLeft;
	Channel32f						mChannelRight;
	CameraPersp						mCamera;
	MayaCamUI						mMayaCam;

	vector<gl::GlslProgRef>			mShader;
	int mShaderNum;
	gl::TextureRef					mTextureLeft;
	gl::TextureRef					mTextureRight;
	gl::Texture::Format				mTextureFormat;
	gl::VboMesh						mMesh;
	uint32_t						mOffset;

	bool							mIsMouseDown;
	bool							mIsAudioPlaying;
	double							mMouseUpTime;
	double							mMouseUpDelay;

	vector<string>					mAudioExtensions;
	fs::path						mAudioPath;
	// audio
	audio::InputDeviceNodeRef		mLineIn;
	audio::MonitorSpectralNodeRef	mMonitorLineInSpectralNode;
	audio::MonitorSpectralNodeRef	mMonitorWaveSpectralNode;

	vector<float>					mMagSpectrum;

	audio::InputDeviceNodeRef		mInputDeviceNode;
	audio::MonitorSpectralNodeRef	mMonitorSpectralNode;
	
	Perlin				mPerlin;
	uint32_t              mPerlinMove;
	std::vector<Vec3f>      mVertices;

	float mFrameRate;
	bool  mAutomaticSwitch;
	ci::params::InterfaceGl		mParams;
	// audio

	//audio::SamplePlayerNodeRef		mSamplePlayerNode;
	//audio::SourceFileRef			mSourceFile;
	float						*mData;
	float						maxVolume;
	bool						mUseLineIn;
	float						mAudioMultFactor;
	float						iFreqs[4];

};

// -------- SPOUT -------------
void ReymentaAudioVisualizerApp::prepareSettings(Settings *settings)
{
        g_Width  = 640;
        g_Height = 512;
        settings->setWindowSize( g_Width, g_Height );
        settings->setFullScreen( false );
        settings->setResizable( false ); // keep the screen size constant for a sender
        settings->setFrameRate( 60.0f );
}
// ----------------------------
void ReymentaAudioVisualizerApp::mouseWheel(MouseEvent event)
{
	// Zoom in/out with mouse wheel
	Vec3f eye = mCamera.getEyePoint();
	eye.z += event.getWheelIncrement();
	mCamera.setEyePoint(eye);
}
void ReymentaAudioVisualizerApp::setup()
{
	maxVolume = 0.0f;
	mUseLineIn = true;
	mAudioMultFactor = 100.0f;
	mData = new float[1024];
	for (int i = 0; i < 1024; i++)
	{
		mData[i] = 0;
	}
	for (int i = 0; i < 4; i++)
	{
		iFreqs[i] = i;
	}
	mPerlin = Perlin(4, 0);
	mPerlinMove = 0;
	mFrameRate = 0.0f;
	mParams = params::InterfaceGl("Params", Vec2i(200, 100));
	mParams.addParam("Frame rate", &mFrameRate, "", true);
	mParams.addParam("Shader", &mShaderNum, "min=0 max=3 step=1", false);
	mParams.addParam("Auto switch", &mAutomaticSwitch);

	// linein
	auto ctx = audio::Context::master();
	mLineIn = ctx->createInputDeviceNode();

	auto scopeLineInFmt = audio::MonitorSpectralNode::Format().fftSize(2048).windowSize(1024);
	mMonitorLineInSpectralNode = ctx->makeNode(new audio::MonitorSpectralNode(scopeLineInFmt));

	mLineIn >> mMonitorLineInSpectralNode;

	mLineIn->enable();

	// wave
	// TODO: it is pretty surprising when you recreate mScope here without checking if there has already been one added.
	//	- user will no longer see the old mScope, but the context still owns a reference to it, so another gets added each time we call this method.
	auto scopeWaveFmt = audio::MonitorSpectralNode::Format().fftSize(2048).windowSize(1024);
	mMonitorWaveSpectralNode = ctx->makeNode(new audio::MonitorSpectralNode(scopeWaveFmt));

	ctx->enable();
	// initialize signals
	signalChannelEnd = false;

	// make a list of valid audio file extensions and initialize audio variables
	const char* extensions[] = { "mp3", "wav", "ogg" };
	mAudioExtensions = vector<string>(extensions, extensions + 2);
	mAudioPath = getAssetPath("");
	mIsAudioPlaying = false;

	for (const auto &dev : audio::Device::getInputDevices()) {
		std::cout << dev->getName() << endl;
	}

	std::vector<audio::DeviceRef> devices = audio::Device::getInputDevices();
	//const auto dev = audio::Device::findDeviceByName(INPUT_DEVICE);
	//if (!dev){
		//cout << "Could not find " << INPUT_DEVICE << endl;
		mInputDeviceNode = ctx->createInputDeviceNode();
		cout << "Using default input" << endl;
	/*}
	else {
		mInputDeviceNode = ctx->createInputDeviceNode(dev);
	}*/

	// By providing an FFT size double that of the window size, we 'zero-pad' the analysis data, which gives
	// an increase in resolution of the resulting spectrum data.
	auto monitorFormat = audio::MonitorSpectralNode::Format().fftSize(kBands).windowSize(kBands / 2);
	mMonitorSpectralNode = ctx->makeNode(new audio::MonitorSpectralNode(monitorFormat));

	mInputDeviceNode >> mMonitorSpectralNode;

	// InputDeviceNode (and all InputNode subclasses) need to be enabled()'s to process audio. So does the Context:
	mInputDeviceNode->enable();
	ctx->enable();

	getWindow()->setTitle(mInputDeviceNode->getDevice()->getName());

	// setup camera
	mCamera.setPerspective(50.0f, 1.0f, 1.0f, 10000.0f);
	mCamera.setEyePoint(Vec3f(-kWidth / 2, kHeight / 2, -kWidth / 8));
	mCamera.setEyePoint(Vec3f(10239.3, 7218.58, -7264.48));
	mCamera.setCenterOfInterestPoint(Vec3f(kWidth*0.5f, -kHeight*0.5f, kWidth*0.5f));

	// create channels from which we can construct our textures
	mChannelLeft = Channel32f(kBands, kHistory);
	mChannelRight = Channel32f(kBands, kHistory);
	memset(mChannelLeft.getData(), 0, mChannelLeft.getRowBytes() * kHistory);
	memset(mChannelRight.getData(), 0, mChannelRight.getRowBytes() * kHistory);

	// create texture format (wrap the y-axis, clamp the x-axis)
	mTextureFormat.setWrapS(GL_CLAMP);
	mTextureFormat.setWrapT(GL_REPEAT);
	mTextureFormat.setMinFilter(GL_LINEAR);
	mTextureFormat.setMagFilter(GL_LINEAR);

	mShaderNum = 0;
	try {
		mShader.push_back(gl::GlslProg::create(loadResource(GLSL_VERT1), loadResource(GLSL_FRAG1)));
		mShader.push_back(gl::GlslProg::create(loadResource(GLSL_VERT1), loadResource(GLSL_FRAG2)));
		mShader.push_back(gl::GlslProg::create(loadResource(GLSL_VERT2), loadResource(GLSL_FRAG1)));
		mShader.push_back(gl::GlslProg::create(loadResource(GLSL_VERT2), loadResource(GLSL_FRAG2)));
	}
	catch (const std::exception& e) {
		console() << e.what() << std::endl;
		quit();
		return;
	}

	std::vector<Colorf>     colors;
	std::vector<Vec2f>      coords;
	std::vector<uint32_t>	indices;

	for (size_t h = 0; h<kHeight; ++h)
	{
		for (size_t w = 0; w<kWidth; ++w)
		{
			// add polygon indices
			if (h < kHeight - 1 && w < kWidth - 1)
			{
				size_t offset = mVertices.size();

				indices.push_back(offset);
				indices.push_back(offset + kWidth);
				indices.push_back(offset + kWidth + 1);
				indices.push_back(offset);
				indices.push_back(offset + kWidth + 1);
				indices.push_back(offset + 1);
			}

			// add vertex
			float value = 80.0f * mPerlin.fBm(Vec3f(float(h), float(w), 0.f) * 0.005f);
			mVertices.push_back(Vec3f(float(w), value, float(h)));

			// add texture coordinates
			// note: we only want to draw the lower part of the frequency bands,
			//  so we scale the coordinates a bit
			const float part = 0.5f;
			float s = w / float(kWidth - 1);
			float t = h / float(kHeight - 1);
			coords.push_back(Vec2f(part - part * s, t));

			// add vertex colors
			colors.push_back(h % 2 == 0 || true ? Color(CM_HSV, s, 1.0f, 1.0f) : Color(CM_RGB, s, s, s));
		}
	}

	gl::VboMesh::Layout layout;
	layout.setStaticPositions();
	//    layout.setDynamicPositions();
	layout.setStaticColorsRGB();
	layout.setStaticIndices();
	layout.setStaticTexCoords2d();

	mMesh = gl::VboMesh(mVertices.size(), indices.size(), layout, GL_TRIANGLES);

	mMesh.bufferPositions(mVertices);
	mMesh.bufferColorsRGB(colors);
	mMesh.bufferIndices(indices);
	mMesh.bufferTexCoords2d(0, coords);

	//    gl::VboMesh::VertexIter iter = mMesh.mapVertexBuffer();
	//    for( int idx = 0; idx < mMesh.getNumVertices(); ++idx ) {
	//        iter.setPosition(mVertices [idx]);
	//        ++iter;
	//    }

	vector<Vec3f> normals;

	// Iterate through again to set normals
	//    for ( int32_t y = 0; y < mResolution.y - 1; y++ ) {
	//        for ( int32_t x = 0; x < mResolution.x - 1; x++ ) {
	//            Vec3f vert0 = positions[ indices[ ( x + mResolution.x * y ) * 6 ] ];
	//            Vec3f vert1 = positions[ indices[ ( ( x + 1 ) + mResolution.x * y ) * 6 ] ];
	//            Vec3f vert2 = positions[ indices[ ( ( x + 1 ) + mResolution.x * ( y + 1 ) ) * 6 ] ];
	//            normals[ x + mResolution.x * y ] = Vec3f( ( vert1 - vert0 ).cross( vert1 - vert2 ).normalized() );
	//        }
	//    }

	mIsMouseDown = false;
	mMouseUpDelay = 5.0;
	mMouseUpTime = getElapsedSeconds() - mMouseUpDelay;

	// the texture offset has two purposes:
	//  1) it tells us where to upload the next spectrum data
	//  2) we use it to offset the texture coordinates in the shader for the scrolling effect
	mOffset = 0;

	setFrameRate(30.0f);

	//fog
	GLfloat density = 0.1;
	GLfloat fogColor[4] = { 0.5, 0.5, 0.5, 1.0 };
	glEnable(GL_DEPTH_TEST); //enable the depth testing
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_EXP2);
	glFogfv(GL_FOG_COLOR, fogColor);
	glFogf(GL_FOG_DENSITY, density);
	glHint(GL_FOG_HINT, GL_NICEST);


    // -------- SPOUT -------------
    // Set up the texture we will use to send out
    // We grab the screen so it has to be the same size
    spoutTexture =  gl::Texture::create(g_Width, g_Height);
    strcpy_s(SenderName, "Reymenta Audio visualizer Sender"); // we have to set a sender name first
    // Optionally test for texture share compatibility
    // bMemoryMode informs us whether Spout initialized for texture share or memory share
    bMemoryMode = spoutsender.GetMemoryShareMode();
    // Initialize a sender
    bInitialized = spoutsender.CreateSender(SenderName, g_Width, g_Height);
    // ----------------------------
}

void ReymentaAudioVisualizerApp::update()
{
	mFrameRate = getAverageFps();

	mMagSpectrum = mMonitorSpectralNode->getMagSpectrum();

	// get spectrum for left and right channels and copy it into our channels
	float* pDataLeft = mChannelLeft.getData() + kBands * mOffset;
	float* pDataRight = mChannelRight.getData() + kBands * mOffset;

	std::reverse_copy(mMagSpectrum.begin(), mMagSpectrum.end(), pDataLeft);
	std::copy(mMagSpectrum.begin(), mMagSpectrum.end(), pDataRight);

	// increment texture offset
	mOffset = (mOffset + 1) % kHistory;

	// clear the spectrum for this row to avoid old data from showing up
	////NOT SURE THIS IS NEEDED -- the texture will be overwritten completely next time by data
	//    pDataLeft = mChannelLeft.getData() + kBands * mOffset;
	//    pDataRight = mChannelRight.getData() + kBands * mOffset;
	//    memset( pDataLeft, 0, kBands * sizeof(float) );
	//    memset( pDataRight, 0, kBands * sizeof(float) );

	// animate camera if mouse has not been down for more than 30 seconds

	if (true || !mIsMouseDown && (getElapsedSeconds() - mMouseUpTime) > mMouseUpDelay)
	{

		float t = float(getElapsedSeconds());
		float x = 0.5f * math<float>::cos(t * 0.07f);
		float y = 0.5f * math<float>::sin(t * 0.09f);//0.1f - 0.2f * math<float>::sin( t * 0.09f );
		float z = 0.05f * math<float>::sin(t * 0.05f) - 0.15f;

		Vec3f eye = Vec3f(kWidth * x, kHeight * y*0.1f, kHeight * z);

		x = 1.0f - x;
		y = -0.5f;
		z = 0.6f + 0.2f *  math<float>::sin(t * 0.12f);

		Vec3f interest = Vec3f(kWidth * x, kHeight * y*0.1f, kHeight * z);
		//cout<<interest<< " "<< (eye.lerp(0.995f, mCamera.getEyePoint()))<<endl;

		// gradually move to eye position and center of interest
		float correction = 1.0 - 0.1*mMonitorSpectralNode->getVolume();
		mCamera.setEyePoint(eye.lerp(0.995f*correction, mCamera.getEyePoint()));
		mCamera.setCenterOfInterestPoint(interest.lerp(0.990f*correction, mCamera.getCenterOfInterestPoint()));


		if (mAutomaticSwitch && (mMonitorSpectralNode->getVolume() < 0.001f || mMonitorSpectralNode->getVolume() > 0.5f)){
			mShaderNum = mShaderNum == mShader.size() - 1 ? 0 : mShaderNum + 1;
		}
	}

	mPerlinMove++;

	for (size_t h = 0; h < kHeight; ++h) {
		for (size_t w = 0; w < kWidth; ++w) {
			size_t i = h * kWidth + w;
			if (w < kWidth - 1) {
				mVertices[i].y = mVertices[i + 1].y;
			}
			else {
				float value = 80.0f*mPerlin.fBm(Vec3f(float(h + mPerlinMove), float(w), 0.f)* 0.005f);
				mVertices[i].y = value;
			}
		}
	}

	mMesh.bufferPositions(mVertices);

	//    gl::VboMesh::VertexIter iter = mMesh.mapVertexBuffer();
	//    int w = 0;
	//    int h = 0;
	//    for( int idx = 0; idx < mMesh.getNumVertices(); ++idx ) {
	//        if (w < kWidth -1) {
	//            iter.setPosition(w,mVertices [idx+1].y, h);
	//        } else {
	//            float value = 80.0f*mPerlin.fBm(Vec3f(float(h+ mPerlinMove), float(w), 0.f)* 0.005f);
	//            iter.setPosition( w,value, h);
	//        }
	//        ++iter;
	//        ++w;
	//        if ( w == kWidth){
	//            w = 0;
	//            h++;
	//        }
	//    }
}

void ReymentaAudioVisualizerApp::draw()
{
	gl::clear();
	gl::enableDepthRead();
	gl::enableDepthWrite();
	// use camera
	gl::pushMatrices();
	gl::setMatrices(mCamera);
	{

		// bind shader
		mShader[mShaderNum]->bind();
		float offSt = mOffset / float(kHistory);
		mShader[mShaderNum]->uniform("uTexOffset", offSt);
		mShader[mShaderNum]->uniform("uLeftTex", 0);
		mShader[mShaderNum]->uniform("uRightTex", 1);
		mShader[mShaderNum]->uniform("resolution", 0.5f*(float)kWidth);

		// create textures from our channels and bind them
		mTextureLeft = gl::Texture::create(mChannelLeft, mTextureFormat);
		mTextureRight = gl::Texture::create(mChannelRight, mTextureFormat);

		mTextureLeft->enableAndBind();
		mTextureRight->bind(1);

		// draw mesh using additive blending
		gl::enableAdditiveBlending();

		gl::color(Color(1, 1, 1));
		gl::draw(mMesh);

		gl::disableAlphaBlending();

		// unbind textures and shader
		mTextureRight->unbind();
		mTextureLeft->unbind();
		mShader[mShaderNum]->unbind();
	}

	gl::popMatrices();
	gl::disableDepthRead();
	gl::disableDepthWrite();

	mParams.draw();


    // -------- SPOUT -------------
    if(bInitialized) {

        // Grab the screen (current read buffer) into the local spout texture
        spoutTexture->bind();
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, g_Width, g_Height);
        spoutTexture->unbind();

        // Send the texture for all receivers to use
        // NOTE : if SendTexture is called with a framebuffer object bound, that binding will be lost
        // and has to be restored afterwards because Spout uses an fbo for intermediate rendering
		spoutsender.SendTexture(spoutTexture->getId(), spoutTexture->getTarget(), g_Width, g_Height);

    }

    // Show the user what it is sending
    char txt[256];
    sprintf_s(txt, "Sending as [%s]", SenderName);
    gl::setMatricesWindow( getWindowSize() );
    gl::enableAlphaBlending();
    gl::drawString( txt, Vec2i( toPixels( 20 ), toPixels( 20 ) ), Color( 1, 1, 1 ), Font( "Verdana", toPixels( 24 ) ) );
    sprintf_s(txt, "fps : %2.2d", (int)getAverageFps());
	gl::drawString(txt, Vec2i(getWindowWidth() - toPixels(100), toPixels(20)), Color(1, 1, 1), Font("Verdana", toPixels(24)));
    gl::disableAlphaBlending();
    // ----------------------------
}
void ReymentaAudioVisualizerApp::fileDrop(FileDropEvent event)
{
	string ext = "";
	// use the last of the dropped files
	boost::filesystem::path mPath = event.getFile(event.getNumFiles() - 1);
	//string mFile = event.getFile(event.getNumFiles() - 1).string();
	string mFile = mPath.string();
	if (mFile.find_last_of(".") != std::string::npos) ext = mFile.substr(mFile.find_last_of(".") + 1);
	if (ext == "wav" || ext == "mp3")
	{
		loadWaveFile(mFile);
		getWindow()->setTitle("Reymenta - " + mFile);
	}
}
void ReymentaAudioVisualizerApp::loadWaveFile(string aFilePath)
{

}
void ReymentaAudioVisualizerApp::mouseDown(MouseEvent event)
{
	// handle mouse down
	mIsMouseDown = true;

	mMayaCam.setCurrentCam(mCamera);
	mMayaCam.mouseDown(event.getPos());
}

void ReymentaAudioVisualizerApp::mouseDrag(MouseEvent event)
{
	// handle mouse drag
	mMayaCam.mouseDrag(event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown());
	mCamera = mMayaCam.getCamera();
}

void ReymentaAudioVisualizerApp::mouseUp(MouseEvent event)
{
	// handle mouse up
	mMouseUpTime = getElapsedSeconds();
	mIsMouseDown = false;
}
// -------- SPOUT -------------
void ReymentaAudioVisualizerApp::shutdown()
{
    spoutsender.ReleaseSender();
}
void ReymentaAudioVisualizerApp::keyDown(KeyEvent event)
{
	// handle key down
	switch (event.getCode())
	{
	case KeyEvent::KEY_ESCAPE:
		quit();
		break;
	case KeyEvent::KEY_F4:
		if (event.isAltDown())
			quit();
		break;
	case KeyEvent::KEY_LEFT:

		break;
	case KeyEvent::KEY_RIGHT:
		break;
	case KeyEvent::KEY_f:
		setFullScreen(!isFullScreen());
		break;
	case KeyEvent::KEY_o:
		break;
	case KeyEvent::KEY_p:
		break;
	case KeyEvent::KEY_s:

		break;
	}
}

void ReymentaAudioVisualizerApp::resize()
{
	// handle resize
	mCamera.setAspectRatio(getWindowAspectRatio());
}
// This line tells Cinder to actually create the application
CINDER_APP_NATIVE( ReymentaAudioVisualizerApp, RendererGl )

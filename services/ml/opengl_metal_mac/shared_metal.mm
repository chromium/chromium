/*
 Copyright © 2018 Apple Inc.

 Permission is hereby granted, free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to deal in
 the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do
 so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#import "shared_metal.h"

// #import <OpenGL/OpenGL.h>
// #import <OpenGL/gl.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#import <OpenGL/gl3.h>

#include "base/logging.h"
#import "opengl_renderer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"

@implementation SharedMetal {
  NSOpenGLContext* _openGLContext;

  OpenGLRenderer* _openGLRenderer;

  GLuint _frameBufferObject;
}

@synthesize interopTexture = _interopTexture;

- (nonnull instancetype)initWithTextureSize:(CGSize)size {
  self = [super init];
  // Create OpenGL context.
  NSOpenGLPixelFormatAttribute attrs[] = {NSOpenGLPFAAccelerated, 0};
  NSOpenGLPixelFormat* pixelFormat =
      [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
  if (!pixelFormat) {
    LOG(ERROR) << "No OpenGL pixel format found";
  }
  _openGLContext =
      [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];

  // After a Metal device has been retrieved and an OpenGL context has been
  // created and made current, a interop texture can be created
  _interopTexture = [[InteroperableTexture alloc]
      initWithMetalDevice:MTLCreateSystemDefaultDevice()
            openGLContext:_openGLContext
         metalPixelFormat:MTLPixelFormatBGRA8Unorm_sRGB
                     size:size];

  // Make OpenGL context current to before issuing and OpenGL command.
  [_openGLContext makeCurrentContext];
  // Create a "frameBufferObject" with the interop texture as the color buffer.
  glGenFramebuffers(1, &_frameBufferObject);
  glBindFramebuffer(GL_FRAMEBUFFER, _frameBufferObject);
  // macOS CVPixelBuffer textures created as rectangle textures.
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_RECTANGLE, _interopTexture.openGLTexture,
                         0);

  // Initialize the openGL render.
  _openGLRenderer = [[OpenGLRenderer alloc] initWithDefault];

  // Set initial OpenGL rendering size to interop texture size.
  glViewport(0, 0, size.width, size.height);

  return self;
}

- (void)drawOpenGLTexture:(GLenum)texTarget texName:(GLuint)texName {
  [_openGLContext makeCurrentContext];

  // Execute OpenGL renderer draw routine to build.
  [_openGLRenderer draw:_frameBufferObject texTarget:texTarget texName:texName];

  // When rendering to a CVPixelBuffer with OpenGL, call glFlush to ensure
  // OpenGL commands are excuted on the pixel buffer before Metal reads the
  // buffer.
  glFlush();
}

- (void)API_AVAILABLE(macosx(10.13))testSharedMetal {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);
  CGImageRef cg_image(SkCreateCGImageRefWithColorspace(bitmap, NULL));

  NSError* error;
  GLKTextureInfo* texInfo =
      [GLKTextureLoader textureWithCGImage:cg_image options:nil error:&error];
  if (!texInfo || error) {
    LOG(ERROR) << error.localizedDescription;
  }

  [self drawOpenGLTexture:texInfo.target texName:texInfo.name];

  id<MTLTexture> new_texture = [_interopTexture.metalTexture
      newTextureViewWithPixelFormat:MTLPixelFormatBGRA8Unorm];
  MPSImage* image =
      [[MPSImage alloc] initWithTexture:new_texture featureChannels:3];
  DLOG(INFO) << image.width;
}

@end

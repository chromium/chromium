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

#import "interoperable_texture.h"

#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
// #import <OpenGL/gl3.h>
#import <CoreVideo/CVMetalTextureCache.h>

typedef struct {
  int cvPixelFormat;
  API_AVAILABLE(macosx(10.11)) MTLPixelFormat mtlFormat;
  GLuint glInternalFormat;
  GLuint glFormat;
  GLuint glType;
} TextureFormatInfo;

// Table of equivalent formats across CoreVideo, Metal, and OpenGL
static const API_AVAILABLE(macosx(10.11))
    TextureFormatInfo InteropFormatTable[] = {
        // Core Video Pixel Format,               Metal Pixel Format, GL
        // internalformat, GL format,   GL type
        {kCVPixelFormatType_32BGRA, MTLPixelFormatBGRA8Unorm, GL_RGBA,
         GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV},
        // {kCVPixelFormatType_ARGB2101010LEPacked, MTLPixelFormatBGR10A2Unorm,
        //  GL_RGB10_A2, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV},
        {kCVPixelFormatType_32BGRA, MTLPixelFormatBGRA8Unorm_sRGB,
         GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV},
        {kCVPixelFormatType_64RGBAHalf, MTLPixelFormatRGBA16Float, GL_RGBA,
         GL_RGBA, GL_HALF_FLOAT},
};

static const API_AVAILABLE(macosx(10.11)) NSUInteger NumInteropFormats =
    sizeof(InteropFormatTable) / sizeof(TextureFormatInfo);

API_AVAILABLE(macosx(10.11))
const TextureFormatInfo* textureFormatInfoFromMetalPixelFormat(
    MTLPixelFormat pixelFormat) {
  for (size_t i = 0; i < NumInteropFormats; i++) {
    if (pixelFormat == InteropFormatTable[i].mtlFormat) {
      return &InteropFormatTable[i];
    }
  }
  return NULL;
}

@implementation InteroperableTexture {
  const TextureFormatInfo* _formatInfo;
  CVPixelBufferRef _CVPixelBuffer;

  NSOpenGLContext* _openGLContext;
  CGLPixelFormatObj _CGLPixelFormat;

  // Metal
  id<MTLDevice> _metalDevice;

  CGSize _size;
}

@synthesize openGLTexture = _openGLTexture;
@synthesize metalTexture = _metalTexture;

- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)metalevice
                              openGLContext:(nonnull NSOpenGLContext*)glContext
                           metalPixelFormat:(MTLPixelFormat)mtlPixelFormat
                                       size:(CGSize)size {
  self = [super init];

  _formatInfo = textureFormatInfoFromMetalPixelFormat(mtlPixelFormat);
  if (!_formatInfo) {
    // LOG(ERROR) << "Metal Format supplied not supported in this sample";
    return nil;
  }

  _size = size;
  _metalDevice = metalevice;
  _openGLContext = glContext;
  _CGLPixelFormat = _openGLContext.pixelFormat.CGLPixelFormatObj;

  NSDictionary* cvBufferProperties = @{
    (__bridge NSString*)kCVPixelBufferOpenGLCompatibilityKey : @YES,
    (__bridge NSString*)kCVPixelBufferMetalCompatibilityKey : @YES,
  };
  CVReturn cvret = CVPixelBufferCreate(
      kCFAllocatorDefault, size.width, size.height, _formatInfo->cvPixelFormat,
      (__bridge CFDictionaryRef)cvBufferProperties, &_CVPixelBuffer);
  if (cvret != kCVReturnSuccess) {
    // LOG(ERROR) << "Failed to create CVPixelBufferf";
    return nil;
  }

  [self createGLTexture];
  [self createMetalTexture];

  return self;
}

/**
 On macOS, create an OpenGL texture and retrieve an OpenGL texture name using
 the following steps, and as annotated in the code listings below:
 */
- (BOOL)createGLTexture {
  CVOpenGLTextureCacheRef _CVGLTextureCache;
  // 1. Create an OpenGL CoreVideo texture cache from the pixel buffer.
  CVReturn cvret = CVOpenGLTextureCacheCreate(
      kCFAllocatorDefault, nil, _openGLContext.CGLContextObj, _CGLPixelFormat,
      nil, &_CVGLTextureCache);
  if (cvret != kCVReturnSuccess) {
    // LOG(ERROR) << "Failed to create OpenGL Texture Cache";
    return NO;
  }
  // 2. Create a CVPixelBuffer-backed OpenGL texture image from the texture
  // cache.
  CVOpenGLTextureRef _CVGLTexture;
  cvret = CVOpenGLTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, _CVGLTextureCache, _CVPixelBuffer, nil,
      &_CVGLTexture);
  if (cvret != kCVReturnSuccess) {
    // LOG(ERROR) << "Failed to create OpenGL Texture From Image";
    return NO;
  }
  // 3. Get an OpenGL texture name from the CVPixelBuffer-backed OpenGL texture
  // image.
  _openGLTexture = CVOpenGLTextureGetName(_CVGLTexture);

  return YES;
}

/**
 Create a Metal texture from the CoreVideo pixel buffer using the following
 steps, and as annotated in the code listings below:
 */
- (BOOL)createMetalTexture {
  CVMetalTextureCacheRef CVMTLTextureCache;
  // 1. Create a Metal Core Video texture cache from the pixel buffer.
  CVReturn cvret = CVMetalTextureCacheCreate(
      kCFAllocatorDefault, nil, _metalDevice, nil, &CVMTLTextureCache);
  if (cvret != kCVReturnSuccess) {
    return NO;
  }
  // 2. Create a CoreVideo pixel buffer backed Metal texture image from the
  // texture cache.
  CVMetalTextureRef CVMTLTexture;
  cvret = CVMetalTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, CVMTLTextureCache, _CVPixelBuffer, nil,
      _formatInfo->mtlFormat, _size.width, _size.height, 0, &CVMTLTexture);
  if (cvret != kCVReturnSuccess) {
    // LOG(ERROR) << "Failed to create Metal texture cache";
    return NO;
  }
  // 3. Get a Metal texture object from the Core Video pixel buffer backed Metal
  // texture image.
  _metalTexture = CVMetalTextureGetTexture(CVMTLTexture);
  if (!_metalTexture) {
    // LOG(ERROR) << "Failed to get metal texture from CVMetalTextureRef";
    return NO;
  }

  return YES;
}

@end

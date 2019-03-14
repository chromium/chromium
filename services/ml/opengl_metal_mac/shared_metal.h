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

#ifndef SERVICES_ML_OPENGL_METAL_MAC_SHARED_METAL_H_
#define SERVICES_ML_OPENGL_METAL_MAC_SHARED_METAL_H_

#import <Foundation/Foundation.h>

#include "base/mac/availability.h"
#import "interoperable_texture.h"

API_AVAILABLE(macosx(10.13))
@interface SharedMetal : NSObject

- (nonnull instancetype)initWithTextureSize:(CGSize)size;

- (void)drawOpenGLTexture:(GLenum)texTarget texName:(GLuint)texName;

- (void)testSharedMetal;

@property(readonly, nonnull, nonatomic) InteroperableTexture* interopTexture;

@end

#endif  // SERVICES_ML_OPENGL_METAL_MAC_SHARED_METAL_H_

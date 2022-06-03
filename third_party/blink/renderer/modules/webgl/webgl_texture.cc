/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLTexture::WebGLTexture(WebGLRenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx), target_(0) {
  GLuint texture;
  ctx->ContextGL()->GenTextures(1, &texture);
  SetObject(texture);
}

WebGLTexture::WebGLTexture(WebGLRenderingContextBase* ctx,
                           GLuint texture,
                           GLenum target)
    : WebGLSharedPlatform3DObject(ctx), target_(target) {
  SetObject(texture);
}

WebGLTexture::~WebGLTexture() = default;

void WebGLTexture::SetTarget(GLenum target) {
  if (!Object())
    return;
  // Target is finalized the first time bindTexture() is called.
  if (target_)
    return;
  target_ = target;
}

void WebGLTexture::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  gl->DeleteTextures(1, &object_);
  object_ = 0;
}

int WebGLTexture::MapTargetToIndex(GLenum target) const {
  if (target_ == GL_TEXTURE_2D) {
    if (target == GL_TEXTURE_2D)
      return 0;
  } else if (target_ == GL_TEXTURE_CUBE_MAP) {
    switch (target) {
      case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        return 0;
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        return 1;
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        return 2;
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        return 3;
      case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        return 4;
      case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return 5;
    }
  } else if (target_ == GL_TEXTURE_3D) {
    if (target == GL_TEXTURE_3D)
      return 0;
  } else if (target_ == GL_TEXTURE_2D_ARRAY) {
    if (target == GL_TEXTURE_2D_ARRAY)
      return 0;
  }
  return -1;
}

GLint WebGLTexture::ComputeLevelCount(GLsizei width,
                                      GLsizei height,
                                      GLsizei depth) {
  // return 1 + log2Floor(std::max(width, height));
  GLsizei n = std::max(std::max(width, height), depth);
  if (n <= 0)
    return 0;
  GLint log = 0;
  GLsizei value = n;
  for (int ii = 4; ii >= 0; --ii) {
    int shift = (1 << ii);
    GLsizei x = (value >> shift);
    if (x) {
      value = x;
      log += shift;
    }
  }
  DCHECK_EQ(value, 1);
  return log + 1;
}

}  // namespace blink

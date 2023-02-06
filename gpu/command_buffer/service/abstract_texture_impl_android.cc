// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/abstract_texture_impl_android.h"

#include <utility>

#include "build/build_config.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {

AbstractTextureImpl::AbstractTextureImpl(gfx::Size size) {
  const auto target = GL_TEXTURE_EXTERNAL_OES;
  // Create a gles2 Texture.
  GLuint service_id = 0;
  api_ = gl::g_current_gl_context;
  api_->glGenTexturesFn(1, &service_id);
  gl::ScopedTextureBinder binder(target, service_id);
  api_->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api_->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api_->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api_->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  texture_ = gpu::gles2::CreateGLES2TextureWithLightRef(service_id, target);
  gfx::Rect cleared_rect;
  texture_->SetLevelInfo(GL_TEXTURE_EXTERNAL_OES, 0, GL_RGBA, size.width(),
                         size.height(), 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         cleared_rect);
  texture_->SetImmutable(true, false);
}

AbstractTextureImpl::~AbstractTextureImpl() {
  // If context is not lost, then the texture should be destroyed on same
  // context it was create on.
  if (have_context_) {
    DCHECK_EQ(api_, gl::g_current_gl_context);
  }

  texture_->RemoveLightweightRef(have_context_);
}

TextureBase* AbstractTextureImpl::GetTextureBase() const {
  return texture_;
}

void AbstractTextureImpl::BindToServiceId(GLuint service_id) {
  texture_->BindToServiceId(service_id);
  texture_->SetLevelCleared(texture_->target(), /*level=*/0, true);
}

void AbstractTextureImpl::NotifyOnContextLost() {
  have_context_ = false;
}

AbstractTextureImplPassthrough::AbstractTextureImplPassthrough(gfx::Size size) {
  const auto target = GL_TEXTURE_EXTERNAL_OES;

  GLuint service_id = 0;
  api_ = gl::g_current_gl_context;
  api_->glGenTexturesFn(1, &service_id);

  GLint prev_texture = 0;
  api_->glGetIntegervFn(gles2::GetTextureBindingQuery(target), &prev_texture);

  api_->glBindTextureFn(target, service_id);
  api_->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api_->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api_->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api_->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(target, prev_texture);

  texture_ = new gles2::TexturePassthrough(service_id, target, GL_RGBA,
                                           size.width(), size.height(), 1, 0,
                                           GL_RGBA, GL_UNSIGNED_BYTE);
}

AbstractTextureImplPassthrough::~AbstractTextureImplPassthrough() {
  // If context is not lost, then the texture should be destroyed on the same
  // context it was create on.
  if (have_context_) {
    DCHECK_EQ(api_, gl::g_current_gl_context);
  }
}

TextureBase* AbstractTextureImplPassthrough::GetTextureBase() const {
  return texture_.get();
}

void AbstractTextureImplPassthrough::BindToServiceId(GLuint service_id) {
  texture_->BindToServiceId(service_id);
}

void AbstractTextureImplPassthrough::NotifyOnContextLost() {
  texture_->MarkContextLost();
  have_context_ = false;
}

}  // namespace gpu

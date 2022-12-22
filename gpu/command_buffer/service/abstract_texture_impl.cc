// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/abstract_texture_impl.h"

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
namespace gles2 {

AbstractTextureImpl::AbstractTextureImpl(GLenum target,
                                         GLenum internal_format,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLenum format,
                                         GLenum type) {
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
  texture_->SetLevelInfo(target, 0, internal_format, width, height, depth,
                         border, format, type, cleared_rect);
  texture_->SetImmutable(true, false);
}

AbstractTextureImpl::~AbstractTextureImpl() {
  // If context is not lost, then the texture should be destroyed on same
  // context it was create on.
  if (have_context_)
    DCHECK_EQ(api_, gl::g_current_gl_context);

  texture_->RemoveLightweightRef(have_context_);
}

TextureBase* AbstractTextureImpl::GetTextureBase() const {
  return texture_;
}

void AbstractTextureImpl::SetParameteri(GLenum pname, GLint param) {
  NOTIMPLEMENTED();
}

#if BUILDFLAG(IS_ANDROID)
void AbstractTextureImpl::BindToServiceId(GLuint service_id) {
  texture_->BindToServiceId(service_id);
  texture_->SetLevelCleared(texture_->target(), /*level=*/0, true);
}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
void AbstractTextureImpl::SetUnboundImage(gl::GLImage* image) {
  NOTIMPLEMENTED();
}
#elif !BUILDFLAG(IS_ANDROID)
void AbstractTextureImpl::SetBoundImage(gl::GLImage* image) {
  NOTIMPLEMENTED();
}
#endif

gl::GLImage* AbstractTextureImpl::GetImageForTesting() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void AbstractTextureImpl::SetCleared() {
  NOTIMPLEMENTED();
}

void AbstractTextureImpl::SetCleanupCallback(CleanupCallback cb) {
  NOTIMPLEMENTED();
}

void AbstractTextureImpl::NotifyOnContextLost() {
  have_context_ = false;
}

AbstractTextureImplPassthrough::AbstractTextureImplPassthrough(
    GLenum target,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type) {
  // Create a gles2 Texture.
  GLuint service_id = 0;
  api_ = gl::g_current_gl_context;
  api_->glGenTexturesFn(1, &service_id);

  GLint prev_texture = 0;
  api_->glGetIntegervFn(GetTextureBindingQuery(target), &prev_texture);

  api_->glBindTextureFn(target, service_id);
  api_->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api_->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api_->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api_->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(target, prev_texture);

  texture_ = new TexturePassthrough(service_id, target, internal_format, width,
                                    height, depth, border, format, type);
}

AbstractTextureImplPassthrough::~AbstractTextureImplPassthrough() {
  // If context is not lost, then the texture should be destroyed on the same
  // context it was create on.
  if (have_context_)
    DCHECK_EQ(api_, gl::g_current_gl_context);
}

TextureBase* AbstractTextureImplPassthrough::GetTextureBase() const {
  return texture_.get();
}

void AbstractTextureImplPassthrough::SetParameteri(GLenum pname, GLint param) {
  NOTIMPLEMENTED();
}

#if BUILDFLAG(IS_ANDROID)
void AbstractTextureImplPassthrough::BindToServiceId(GLuint service_id) {
  texture_->BindToServiceId(service_id);
}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
void AbstractTextureImplPassthrough::SetUnboundImage(gl::GLImage* image) {
  NOTIMPLEMENTED();
}
#elif !BUILDFLAG(IS_ANDROID)
void AbstractTextureImplPassthrough::SetBoundImage(gl::GLImage* image) {
  NOTIMPLEMENTED();
}
#endif

gl::GLImage* AbstractTextureImplPassthrough::GetImageForTesting() const {
  NOTIMPLEMENTED();
  return nullptr;
}

void AbstractTextureImplPassthrough::SetCleared() {
  NOTIMPLEMENTED();
}

void AbstractTextureImplPassthrough::SetCleanupCallback(CleanupCallback cb) {
  NOTIMPLEMENTED();
}

void AbstractTextureImplPassthrough::NotifyOnContextLost() {
  texture_->MarkContextLost();
  have_context_ = false;
}

}  // namespace gles2
}  // namespace gpu

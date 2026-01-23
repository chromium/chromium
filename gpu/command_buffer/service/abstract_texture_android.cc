// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/abstract_texture_android.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace {
GLuint CreateTextureWithLinearFilter() {
  const auto target = GL_TEXTURE_EXTERNAL_OES;
  GLuint service_id = 0;
  auto* api = gl::g_current_gl_context;
  api->glGenTexturesFn(1, &service_id);
  gl::ScopedTextureBinder binder(target, service_id);
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return service_id;
}
}  // namespace

std::unique_ptr<AbstractTextureAndroidValidating>
AbstractTextureAndroidValidating::Create(gfx::Size size) {
  GLuint service_id = CreateTextureWithLinearFilter();

  auto* texture = gpu::gles2::CreateGLES2TextureWithLightRef(
      service_id, GL_TEXTURE_EXTERNAL_OES);
  gfx::Rect cleared_rect;
  texture->SetLevelInfo(GL_TEXTURE_EXTERNAL_OES, 0, GL_RGBA, size.width(),
                        size.height(), 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        cleared_rect);
  texture->SetImmutable(true, false);

  return std::make_unique<AbstractTextureAndroidValidating>(texture);
}

std::unique_ptr<AbstractTextureAndroidPassthrough>
AbstractTextureAndroidPassthrough::Create(gfx::Size size) {
  GLuint service_id = CreateTextureWithLinearFilter();
  auto texture = base::MakeRefCounted<gles2::TexturePassthrough>(
      service_id, GL_TEXTURE_EXTERNAL_OES);

  return std::make_unique<AbstractTextureAndroidPassthrough>(std::move(texture),
                                                             size);
}

AbstractTextureAndroidValidating::AbstractTextureAndroidValidating(
    gles2::Texture* texture)
    : texture_(texture), api_(gl::g_current_gl_context) {}

AbstractTextureAndroidPassthrough::AbstractTextureAndroidPassthrough(
    scoped_refptr<gles2::TexturePassthrough> texture,
    const gfx::Size& size)
    : texture_passthrough_(std::move(texture)),
      texture_passthrough_size_(size),
      api_(gl::g_current_gl_context) {
  DCHECK(texture_passthrough_ &&
         texture_passthrough_->target() == GL_TEXTURE_EXTERNAL_OES);
}

AbstractTextureAndroidValidating::~AbstractTextureAndroidValidating() {
  // If context is not lost, then the texture should be destroyed on same
  // context it was create on.
  if ((texture_) && have_context_) {
    DCHECK_EQ(api_, gl::g_current_gl_context);
  }

  if (texture_) {
    texture_.ExtractAsDangling()->RemoveLightweightRef(have_context_);
  }
}

AbstractTextureAndroidPassthrough::~AbstractTextureAndroidPassthrough() {
  // If context is not lost, then the texture should be destroyed on same
  // context it was create on.
  if ((texture_passthrough_) && have_context_) {
    DCHECK_EQ(api_, gl::g_current_gl_context);
  }
}

void AbstractTextureAndroidValidating::NotifyOnContextLost() {
  have_context_ = false;
}

void AbstractTextureAndroidPassthrough::NotifyOnContextLost() {
  have_context_ = false;
  if (texture_passthrough_) {
    texture_passthrough_->MarkContextLost();
  }
}

TextureBase* AbstractTextureAndroidValidating::GetTextureBase() const {
  return texture_;
}

TextureBase* AbstractTextureAndroidPassthrough::GetTextureBase() const {
  return texture_passthrough_.get();
}

}  // namespace gpu

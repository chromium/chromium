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

std::unique_ptr<AbstractTextureAndroid>
AbstractTextureAndroid::CreateForValidating(gfx::Size size) {
  GLuint service_id = CreateTextureWithLinearFilter();

  auto* texture = gpu::gles2::CreateGLES2TextureWithLightRef(
      service_id, GL_TEXTURE_EXTERNAL_OES);
  gfx::Rect cleared_rect;
  texture->SetLevelInfo(GL_TEXTURE_EXTERNAL_OES, 0, GL_RGBA, size.width(),
                        size.height(), 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        cleared_rect);
  texture->SetImmutable(true, false);

  return std::make_unique<AbstractTextureAndroid>(texture);
}

std::unique_ptr<AbstractTextureAndroid>
AbstractTextureAndroid::CreateForPassthrough(gfx::Size size) {
  GLuint service_id = CreateTextureWithLinearFilter();
  auto texture = base::MakeRefCounted<gles2::TexturePassthrough>(
      service_id, GL_TEXTURE_EXTERNAL_OES);

  return std::make_unique<AbstractTextureAndroid>(std::move(texture), size);
}

std::unique_ptr<AbstractTextureAndroid>
AbstractTextureAndroid::CreateForTesting(GLuint texture_id) {
  auto texture = std::make_unique<gpu::TextureBase>(texture_id);
  return std::make_unique<AbstractTextureAndroid>(std::move(texture));
}

AbstractTextureAndroid::AbstractTextureAndroid(
    std::unique_ptr<gpu::TextureBase> texture)
    : texture_for_testing_(std::move(texture)) {}
AbstractTextureAndroid::AbstractTextureAndroid(gles2::Texture* texture)
    : texture_(texture), api_(gl::g_current_gl_context) {}
AbstractTextureAndroid::AbstractTextureAndroid(
    scoped_refptr<gles2::TexturePassthrough> texture,
    const gfx::Size& size)
    : texture_passthrough_(std::move(texture)),
      texture_passthrough_size_(size),
      api_(gl::g_current_gl_context) {
  DCHECK(texture_passthrough_ &&
         texture_passthrough_->target() == GL_TEXTURE_EXTERNAL_OES);
}

AbstractTextureAndroid::~AbstractTextureAndroid() {
  // If context is not lost, then the texture should be destroyed on same
  // context it was create on.
  if ((texture_ || texture_passthrough_) && have_context_) {
    DCHECK_EQ(api_, gl::g_current_gl_context);
  }

  if (texture_) {
    texture_.ExtractAsDangling()->RemoveLightweightRef(have_context_);
  }
}

void AbstractTextureAndroid::NotifyOnContextLost() {
  if (texture_passthrough_) {
    texture_passthrough_->MarkContextLost();
  }
  have_context_ = false;
}

void AbstractTextureAndroid::BindToServiceId(GLuint service_id) {
  if (texture_) {
    texture_->BindToServiceId(service_id);
    texture_->SetLevelCleared(texture_->target(), /*level=*/0, true);
  } else if (texture_passthrough_) {
    texture_passthrough_->BindToServiceId(service_id);

    if (gl::g_current_gl_driver->ext.b_GL_ANGLE_texture_external_update) {
      // Notify the texture that its size has changed.
      unsigned int target = texture_passthrough_->target();
      GLint prev_texture = 0;
      glGetIntegerv(gles2::GetTextureBindingQuery(target), &prev_texture);
      glBindTexture(target, texture_passthrough_->service_id());

      glTexImage2DExternalANGLE(
          target, /*level=*/0, /*internalformat=*/GL_RGBA,
          texture_passthrough_size_.width(), texture_passthrough_size_.height(),
          /*border=*/0, /*format=*/GL_RGBA, /*type=*/GL_UNSIGNED_BYTE);

      glBindTexture(target, prev_texture);
    }
  }
}

TextureBase* AbstractTextureAndroid::GetTextureBase() const {
  if (texture_) {
    return texture_;
  }
  if (texture_passthrough_) {
    return texture_passthrough_.get();
  }
  if (texture_for_testing_) {
    return texture_for_testing_.get();
  }
  return nullptr;
}

}  // namespace gpu

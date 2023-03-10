// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/validating_abstract_texture_impl.h"

#include "build/build_config.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace gles2 {

ValidatingAbstractTextureImpl::ValidatingAbstractTextureImpl(
    scoped_refptr<TextureRef> texture_ref,
    DecoderContext* decoder_context,
    DestructionCB destruction_cb)
    : texture_ref_(std::move(texture_ref)),
      decoder_context_(decoder_context),
      destruction_cb_(std::move(destruction_cb)) {}

ValidatingAbstractTextureImpl::~ValidatingAbstractTextureImpl() {
  if (cleanup_cb_) {
    DCHECK(texture_ref_);
    std::move(cleanup_cb_).Run(this);
  }

  if (destruction_cb_)
    std::move(destruction_cb_).Run(this, std::move(texture_ref_));

  DCHECK(!texture_ref_);
}

TextureBase* ValidatingAbstractTextureImpl::GetTextureBase() const {
  if (!texture_ref_)
    return nullptr;
  return texture_ref_->texture();
}

void ValidatingAbstractTextureImpl::SetParameteri(GLenum pname, GLint param) {
  if (!texture_ref_)
    return;

  gl::ScopedTextureBinder binder(texture_ref_->texture()->target(),
                                 service_id());
  GetTextureManager()->SetParameteri(__func__, GetErrorState(),
                                     texture_ref_.get(), pname, param);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
void ValidatingAbstractTextureImpl::SetUnboundImage(gl::GLImage* image) {
  if (!texture_ref_)
    return;

  const GLuint target = texture_ref_->texture()->target();
  const GLint level = 0;

  // Configure the new image.
  if (image) {
    GetTextureManager()->SetUnboundLevelImage(texture_ref_.get(), target, level,
                                              image);
  } else {
    GetTextureManager()->UnsetLevelImage(texture_ref_.get(), target, level);
  }

  GetTextureManager()->SetLevelCleared(texture_ref_.get(), target, level,
                                       image);
}
#else
void ValidatingAbstractTextureImpl::SetBoundImage(gl::GLImage* image) {
  if (!texture_ref_) {
    return;
  }

  const GLuint target = texture_ref_->texture()->target();
  const GLint level = 0;

  // Configure the new image.
  if (image) {
    GetTextureManager()->SetBoundLevelImage(texture_ref_.get(), target, level,
                                            image);
  } else {
    GetTextureManager()->UnsetLevelImage(texture_ref_.get(), target, level);
  }

  GetTextureManager()->SetLevelCleared(texture_ref_.get(), target, level,
                                       image);
}
#endif

gl::GLImage* ValidatingAbstractTextureImpl::GetImageForTesting() const {
  if (!texture_ref_)
    return nullptr;

  const GLuint target = texture_ref_->texture()->target();
  const GLint level = 0;
  return texture_ref_->texture()->GetLevelImage(target, level);
}

void ValidatingAbstractTextureImpl::SetCleared() {
  if (!texture_ref_)
    return;

  const GLint level = 0;
  GetTextureManager()->SetLevelCleared(
      texture_ref_.get(), texture_ref_->texture()->target(), level, true);
}

void ValidatingAbstractTextureImpl::SetCleanupCallback(CleanupCallback cb) {
  cleanup_cb_ = std::move(cb);
}

TextureManager* ValidatingAbstractTextureImpl::GetTextureManager() const {
  DCHECK(decoder_context_);
  return GetContextGroup()->texture_manager();
}

ContextGroup* ValidatingAbstractTextureImpl::GetContextGroup() const {
  DCHECK(decoder_context_);
  return decoder_context_->GetContextGroup();
}

ErrorState* ValidatingAbstractTextureImpl::GetErrorState() const {
  DCHECK(decoder_context_);
  return decoder_context_->GetErrorState();
}

void ValidatingAbstractTextureImpl::NotifyOnContextLost() {
  NOTIMPLEMENTED();
}

void ValidatingAbstractTextureImpl::OnDecoderWillDestroy(bool have_context) {
  // If we don't have a context, then notify the TextureRef not to delete itself
  // if this is the last reference.
  destruction_cb_ = DestructionCB();
  decoder_context_ = nullptr;

  // If we already got rid of the texture ref, then there's nothing to do.
  if (!texture_ref_)
    return;

  if (cleanup_cb_)
    std::move(cleanup_cb_).Run(this);

  // If we have no context, then notify the TextureRef in case it's the last
  // ref to the texture.
  if (!have_context)
    texture_ref_->ForceContextLost();
  texture_ref_ = nullptr;
}

TextureRef* ValidatingAbstractTextureImpl::GetTextureRefForTesting() {
  return texture_ref_.get();
}

}  // namespace gles2
}  // namespace gpu

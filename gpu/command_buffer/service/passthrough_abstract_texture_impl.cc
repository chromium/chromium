// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "build/build_config.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/passthrough_abstract_texture_impl.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace gles2 {

PassthroughAbstractTextureImpl::PassthroughAbstractTextureImpl(
    scoped_refptr<TexturePassthrough> texture_passthrough,
    GLES2DecoderPassthroughImpl* decoder)
    : texture_passthrough_(std::move(texture_passthrough)),
      gl_api_(decoder->api()),
      decoder_(decoder) {}

PassthroughAbstractTextureImpl::~PassthroughAbstractTextureImpl() {
  if (cleanup_cb_) {
    DCHECK(texture_passthrough_);
    std::move(cleanup_cb_).Run(this);
  }

  if (decoder_)
    decoder_->OnAbstractTextureDestroyed(this, std::move(texture_passthrough_));
  DCHECK(!texture_passthrough_);
}

TextureBase* PassthroughAbstractTextureImpl::GetTextureBase() const {
  return texture_passthrough_.get();
}

void PassthroughAbstractTextureImpl::SetParameteri(GLenum pname, GLint param) {
  if (!texture_passthrough_)
    return;

  gl::ScopedTextureBinder binder(texture_passthrough_->target(), service_id());
  gl_api_->glTexParameteriFn(texture_passthrough_->target(), pname, param);
}

void PassthroughAbstractTextureImpl::SetCleared() {
  // The passthrough decoder has no notion of 'cleared', so do nothing.
}

void PassthroughAbstractTextureImpl::SetCleanupCallback(CleanupCallback cb) {
  cleanup_cb_ = std::move(cb);
}

void PassthroughAbstractTextureImpl::NotifyOnContextLost() {
  NOTIMPLEMENTED();
}

scoped_refptr<TexturePassthrough>
PassthroughAbstractTextureImpl::OnDecoderWillDestroy() {
  // Make sure that destruction_cb_ does nothing when destroyed, since
  // the DecoderContext is invalid. Also null out invalid pointer.
  DCHECK(texture_passthrough_);

  if (cleanup_cb_)
    std::move(cleanup_cb_).Run(this);

  decoder_ = nullptr;
  gl_api_ = nullptr;
  return std::move(texture_passthrough_);
}

}  // namespace gles2
}  // namespace gpu

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/reflector_texture.h"

#include "content/browser/compositor/owned_mailbox.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"

namespace content {

ReflectorTexture::ReflectorTexture(viz::ContextProvider* context_provider)
    : gl_(context_provider->ContextGL()), texture_id_(0) {
  mailbox_ = new OwnedMailbox(gl_);

  if (!mailbox_->mailbox().IsZero()) {
    if (mailbox_->sync_token().HasData())
      gl_->WaitSyncTokenCHROMIUM(mailbox_->sync_token().GetConstData());
    texture_id_ =
        gl_->CreateAndConsumeTextureCHROMIUM(mailbox_->mailbox().name);
  }
}

ReflectorTexture::~ReflectorTexture() {
  if (texture_id_ != 0)
    gl_->DeleteTextures(1, &texture_id_);
}

void ReflectorTexture::CopyTextureFullImage(const gfx::Size& size) {
  gl_->BindTexture(GL_TEXTURE_2D, texture_id_);
  gl_->CopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, size.width(),
                      size.height(), 0);
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  // Insert a barrier to make the copy show up in the mirroring compositor's
  // mailbox. Since the the compositor contexts and the ImageTransportFactory's
  // GL context are all on the same GPU channel, this is sufficient instead of
  // plumbing through a sync point.
  gl_->OrderingBarrierCHROMIUM();
}

void ReflectorTexture::CopyTextureSubImage(const gfx::Rect& rect) {
  gl_->BindTexture(GL_TEXTURE_2D, texture_id_);
  gl_->CopyTexSubImage2D(GL_TEXTURE_2D, 0, rect.x(), rect.y(), rect.x(),
                         rect.y(), rect.width(), rect.height());
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  // Insert a barrier for the same reason above.
  gl_->OrderingBarrierCHROMIUM();
}

}  // namespace content

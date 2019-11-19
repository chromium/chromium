// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/owned_mailbox.h"

#include "base/bind.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace content {

OwnedMailbox::OwnedMailbox(gpu::gles2::GLES2Interface* gl)
    : gl_(gl), texture_id_(0) {
  DCHECK(gl_);

  // Create the texture.
  static_assert(sizeof(texture_id_) == sizeof(GLuint),
                "need to adjust type of texture_id_ in its declaration");
  gl_->GenTextures(1, &texture_id_);
  gl_->BindTexture(GL_TEXTURE_2D, texture_id_);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->BindTexture(GL_TEXTURE_2D, 0);

  // Initialize the MailboxHolder for the texture.
  gl_->ProduceTextureDirectCHROMIUM(texture_id_, mailbox_holder_.mailbox.name);
  gl_->GenSyncTokenCHROMIUM(mailbox_holder_.sync_token.GetData());
  mailbox_holder_.texture_target = GL_TEXTURE_2D;
}

OwnedMailbox::~OwnedMailbox() {
  gl_->WaitSyncTokenCHROMIUM(mailbox_holder_.sync_token.GetConstData());
  gl_->DeleteTextures(1, &texture_id_);
}

std::unique_ptr<viz::SingleReleaseCallback>
OwnedMailbox::GetSingleReleaseCallback() {
  return viz::SingleReleaseCallback::Create(base::BindOnce(
      &OwnedMailbox::UpdateSyncToken, weak_ptr_factory_.GetWeakPtr()));
}

void OwnedMailbox::UpdateSyncToken(const gpu::SyncToken& sync_token,
                                   bool is_lost) {
  if (sync_token.HasData())
    mailbox_holder_.sync_token = sync_token;
}

}  // namespace content

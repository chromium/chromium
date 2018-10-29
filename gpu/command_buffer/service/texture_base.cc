// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_base.h"

#include "base/logging.h"
#include "gpu/command_buffer/service/mailbox_manager.h"

namespace gpu {

TextureBase::TextureBase(unsigned int service_id)
    : service_id_(service_id), target_(0), mailbox_manager_(nullptr) {}

TextureBase::~TextureBase() {
  DCHECK_EQ(nullptr, mailbox_manager_);
}

void TextureBase::SetTarget(unsigned int target) {
  DCHECK_EQ(0u, target_);  // you can only set this once.
  target_ = target;
}

void TextureBase::DeleteFromMailboxManager() {
  if (mailbox_manager_) {
    mailbox_manager_->TextureDeleted(this);
    mailbox_manager_ = nullptr;
  }
}

void TextureBase::SetMailboxManager(MailboxManager* mailbox_manager) {
  DCHECK(!mailbox_manager_ || mailbox_manager_ == mailbox_manager);
  mailbox_manager_ = mailbox_manager;
}

TextureBase::Type TextureBase::GetType() const {
  return TextureBase::Type::kNone;
}

}  // namespace gpu

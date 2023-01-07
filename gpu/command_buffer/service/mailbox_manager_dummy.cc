// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mailbox_manager_dummy.h"

#include "base/notreached.h"
#include "gpu/command_buffer/service/texture_base.h"

namespace gpu {
namespace gles2 {

MailboxManagerDummy::MailboxManagerDummy() = default;

MailboxManagerDummy::~MailboxManagerDummy() = default;

TextureBase* MailboxManagerDummy::ConsumeTexture(const Mailbox& mailbox) {
  NOTREACHED();
  return nullptr;
}

}  // namespace gles2
}  // namespace gpu

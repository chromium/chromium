// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "gpu/command_buffer/common/mailbox_holder.h"

namespace gpu {

MailboxHolder::MailboxHolder() : texture_target(0) {}

MailboxHolder::MailboxHolder(const gpu::Mailbox& mailbox,
                             const gpu::SyncToken& sync_token,
                             uint32_t texture_target)
    : mailbox(mailbox),
      sync_token(sync_token),
      texture_target(texture_target) {}

}  // namespace gpu

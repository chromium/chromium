// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_MAILBOX_HOLDER_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_MAILBOX_HOLDER_MOJOM_TRAITS_H_

#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/ipc/common/mailbox_holder.mojom-shared.h"
#include "gpu/ipc/common/mailbox_mojom_traits.h"
#include "gpu/ipc/common/sync_token_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::MailboxHolderDataView, gpu::MailboxHolder> {
  static const gpu::Mailbox& mailbox(const gpu::MailboxHolder& holder) {
    return holder.mailbox;
  }

  static const gpu::SyncToken& sync_token(const gpu::MailboxHolder& holder) {
    return holder.sync_token;
  }

  static uint32_t texture_target(const gpu::MailboxHolder& holder) {
    return holder.texture_target;
  }

  static bool Read(gpu::mojom::MailboxHolderDataView data,
                   gpu::MailboxHolder* out) {
    if (!data.ReadMailbox(&out->mailbox) ||
        !data.ReadSyncToken(&out->sync_token))
      return false;
    out->texture_target = data.texture_target();
    return true;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_MAILBOX_HOLDER_MOJOM_TRAITS_H_

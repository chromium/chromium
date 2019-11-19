// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_HANDLE_ATTACHMENT_FUCHSIA_H_
#define IPC_HANDLE_ATTACHMENT_FUCHSIA_H_

#include <lib/zx/handle.h>
#include <stdint.h>

#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_message_support_export.h"

namespace IPC {
namespace internal {

// This class represents a Fuchsia zx_handle_t attached to a Chrome IPC message.
class IPC_MESSAGE_SUPPORT_EXPORT HandleAttachmentFuchsia
    : public MessageAttachment {
 public:
  // This constructor takes ownership of |handle|. Should only be called by the
  // receiver of a Chrome IPC message.
  explicit HandleAttachmentFuchsia(zx::handle handle);

  Type GetType() const override;

  zx_handle_t Take() { return handle_.release(); }

 private:
  ~HandleAttachmentFuchsia() override;

  zx::handle handle_;
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_HANDLE_ATTACHMENT_FUCHSIA_H_

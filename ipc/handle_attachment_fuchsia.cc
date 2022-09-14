// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/handle_attachment_fuchsia.h"

#include <zircon/syscalls.h>
#include <zircon/types.h>

#include "base/fuchsia/fuchsia_logging.h"

namespace IPC {
namespace internal {

HandleAttachmentFuchsia::HandleAttachmentFuchsia(zx::handle handle)
    : handle_(std::move(handle)) {}

HandleAttachmentFuchsia::~HandleAttachmentFuchsia() {}

MessageAttachment::Type HandleAttachmentFuchsia::GetType() const {
  return Type::FUCHSIA_HANDLE;
}

}  // namespace internal
}  // namespace IPC

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_H_

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/gpu_export.h"

namespace gpu {

// Manages resources scoped beyond the context or context group level.
class GPU_EXPORT MailboxManager {
 public:
  virtual ~MailboxManager() = default;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_H_


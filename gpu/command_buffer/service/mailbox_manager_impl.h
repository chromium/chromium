// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_IMPL_H_
#define GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_IMPL_H_

#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
namespace gles2 {

// Manages resources scoped beyond the context or context group level.
class GPU_GLES2_EXPORT MailboxManagerImpl : public MailboxManager {
 public:
  MailboxManagerImpl();

  MailboxManagerImpl(const MailboxManagerImpl&) = delete;
  MailboxManagerImpl& operator=(const MailboxManagerImpl&) = delete;

  ~MailboxManagerImpl() override;
};

}  // namespage gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_IMPL_H_

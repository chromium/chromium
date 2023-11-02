// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_FACTORY_H_

#include <memory>

#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

struct GpuPreferences;

namespace gles2 {

std::unique_ptr<MailboxManager> GPU_GLES2_EXPORT
CreateMailboxManager(const GpuPreferences& gpu_preferences);

}  // namespace gles2
}  // namespace gpu
#endif  // GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_FACTORY_H_

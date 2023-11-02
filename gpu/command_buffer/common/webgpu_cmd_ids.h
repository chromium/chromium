// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the webgpu command buffer commands.

#ifndef GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_IDS_H_
#define GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_IDS_H_

#include "gpu/command_buffer/common/cmd_buffer_common.h"

namespace gpu {
namespace webgpu {

#include "gpu/command_buffer/common/webgpu_cmd_ids_autogen.h"

const char* GetCommandName(CommandId command_id);

using DawnRequestAdapterSerial = uint64_t;
using DawnRequestDeviceSerial = uint64_t;

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_IDS_H_

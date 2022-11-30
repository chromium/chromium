// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_CMD_HELPER_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_CMD_HELPER_H_

#include <stdint.h>

#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/client/webgpu_export.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"

namespace gpu {
namespace webgpu {

// A class that helps write WebGPU command buffers.
class WEBGPU_EXPORT WebGPUCmdHelper : public CommandBufferHelper {
 public:
  explicit WebGPUCmdHelper(CommandBuffer* command_buffer);

  WebGPUCmdHelper(const WebGPUCmdHelper&) = delete;
  WebGPUCmdHelper& operator=(const WebGPUCmdHelper&) = delete;

  ~WebGPUCmdHelper() override;

// Include the auto-generated part of this class. We split this because it
// means we can easily edit the non-auto generated parts right here in this
// file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_cmd_helper_autogen.h"
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_CMD_HELPER_H_

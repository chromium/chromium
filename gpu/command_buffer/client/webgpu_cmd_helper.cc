// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/webgpu_cmd_helper.h"

namespace gpu {
namespace webgpu {

WebGPUCmdHelper::WebGPUCmdHelper(CommandBuffer* command_buffer)
    : CommandBufferHelper(command_buffer) {}

WebGPUCmdHelper::~WebGPUCmdHelper() = default;

}  // namespace webgpu
}  // namespace gpu

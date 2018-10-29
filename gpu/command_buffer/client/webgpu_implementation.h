// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_H_

#include "gpu/command_buffer/client/implementation_base.h"
#include "gpu/command_buffer/client/logging.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_export.h"
#include "gpu/command_buffer/client/webgpu_interface.h"

namespace gpu {
namespace webgpu {

class WEBGPU_EXPORT WebGPUImplementation final : public WebGPUInterface {
 public:
  explicit WebGPUImplementation(WebGPUCmdHelper* helper) : helper_(helper) {}
  ~WebGPUImplementation() override {}

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_implementation_autogen.h"

 private:
  const char* GetLogPrefix() const { return "webgpu"; }

  WebGPUCmdHelper* helper_;
  LogSettings log_settings_;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_H_

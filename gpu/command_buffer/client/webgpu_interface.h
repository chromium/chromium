// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_H_

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu.h>

#include "gpu/command_buffer/client/interface_base.h"
#include "gpu/command_buffer/common/webgpu_cmd_enums.h"

namespace gpu {
namespace webgpu {

struct ReservedTexture {
  WGPUTexture texture;
  uint32_t id;
  uint32_t generation;
};

class WebGPUInterface : public InterfaceBase {
 public:
  WebGPUInterface() {}
  virtual ~WebGPUInterface() {}

  virtual const DawnProcTable& GetProcs() const = 0;
  virtual void FlushCommands() = 0;
  virtual WGPUDevice GetDefaultDevice() = 0;
  virtual ReservedTexture ReserveTexture(WGPUDevice device) = 0;

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_interface_autogen.h"
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_H_

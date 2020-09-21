// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_H_

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu.h>

#include "base/callback.h"
#include "gpu/command_buffer/client/interface_base.h"
#include "gpu/command_buffer/common/webgpu_cmd_enums.h"
#include "gpu/command_buffer/common/webgpu_cmd_ids.h"

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

  // Flush all commands.
  virtual void FlushCommands() = 0;

  // Flush all commands on the device client.
  virtual void FlushCommands(DawnDeviceClientID device_client_id) = 0;

  // Ensure the awaiting flush flag is set on the device client. Returns false
  // if a flush has already been indicated, or a flush is not needed (there may
  // be no commands to flush). Returns true if the caller should schedule a
  // flush.
  virtual void EnsureAwaitingFlush(DawnDeviceClientID device_client_id,
                                   bool* needs_flush) = 0;

  // If the awaiting flush flag is set, flushes commands. Otherwise, does
  // nothing.
  virtual void FlushAwaitingCommands(DawnDeviceClientID device_client_id) = 0;

  virtual WGPUDevice GetDevice(DawnDeviceClientID device_client_id) = 0;
  virtual ReservedTexture ReserveTexture(
      DawnDeviceClientID device_client_id) = 0;
  virtual bool RequestAdapterAsync(
      PowerPreference power_preference,
      base::OnceCallback<void(int32_t, const WGPUDeviceProperties&)>
          request_adapter_callback) = 0;
  virtual bool RequestDeviceAsync(
      uint32_t adapter_service_id,
      const WGPUDeviceProperties& requested_device_properties,
      base::OnceCallback<void(bool, DawnDeviceClientID)>
          request_device_callback) = 0;
  virtual void RemoveDevice(DawnDeviceClientID device_client_id) = 0;

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_interface_autogen.h"
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_H_

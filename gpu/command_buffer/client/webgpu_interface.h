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
  uint32_t deviceId;
  uint32_t deviceGeneration;
};

class WebGPUInterface : public InterfaceBase {
 public:
  WebGPUInterface() {}
  virtual ~WebGPUInterface() {}

  virtual const DawnProcTable& GetProcs() const = 0;

  // Flush all commands.
  virtual void FlushCommands() = 0;

  // Ensure the awaiting flush flag is set on the device client. Returns false
  // if a flush has already been indicated, or a flush is not needed (there may
  // be no commands to flush). Returns true if the caller should schedule a
  // flush.
  virtual void EnsureAwaitingFlush(bool* needs_flush) = 0;

  // If the awaiting flush flag is set, flushes commands. Otherwise, does
  // nothing.
  virtual void FlushAwaitingCommands() = 0;

  // Disconnect. All commands should become a no-op and server-side resources
  // can be freed.
  virtual void DisconnectContextAndDestroyServer() = 0;

  virtual ReservedTexture ReserveTexture(WGPUDevice device) = 0;
  virtual void RequestAdapterAsync(
      PowerPreference power_preference,
      base::OnceCallback<void(int32_t,
                              const WGPUDeviceProperties&,
                              const char*)> request_adapter_callback) = 0;
  virtual void RequestDeviceAsync(
      uint32_t adapter_service_id,
      const WGPUDeviceProperties& requested_device_properties,
      base::OnceCallback<void(WGPUDevice)> request_device_callback) = 0;

  // Gets or creates a usable WGPUDevice synchronously. It really should not
  // be used, and the async request adapter and request device APIs should be
  // used instead.
  virtual WGPUDevice DeprecatedEnsureDefaultDeviceSync() = 0;

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_interface_autogen.h"
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_H_

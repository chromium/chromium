// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_STUB_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_STUB_H_

#include "gpu/command_buffer/client/webgpu_interface.h"

namespace gpu {
namespace webgpu {

// This class a stub to help with mocks for the WebGPUInterface class.
class WebGPUInterfaceStub : public WebGPUInterface {
 public:
  WebGPUInterfaceStub();
  ~WebGPUInterfaceStub() override;

  // InterfaceBase implementation.
  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override;
  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override;

  // WebGPUInterface implementation
  const DawnProcTable& GetProcs() const override;
  void FlushCommands() override;
  void EnsureAwaitingFlush(bool* needs_flush) override;
  void FlushAwaitingCommands() override;
  void DisconnectContextAndDestroyServer() override;
  ReservedTexture ReserveTexture(WGPUDevice device) override;
  void RequestAdapterAsync(
      PowerPreference power_preference,
      base::OnceCallback<void(int32_t,
                              const WGPUDeviceProperties&,
                              const char*)> request_adapter_callback) override;
  void RequestDeviceAsync(
      uint32_t adapter_service_id,
      const WGPUDeviceProperties& requested_device_properties,
      base::OnceCallback<void(WGPUDevice)> request_device_callback) override;

  WGPUDevice DeprecatedEnsureDefaultDeviceSync() override;

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_interface_stub_autogen.h"

 private:
  DawnProcTable null_procs_;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_STUB_H_

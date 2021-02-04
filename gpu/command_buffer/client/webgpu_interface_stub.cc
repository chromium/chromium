// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/webgpu_interface_stub.h"

namespace gpu {
namespace webgpu {

WebGPUInterfaceStub::WebGPUInterfaceStub() = default;

WebGPUInterfaceStub::~WebGPUInterfaceStub() = default;

// InterfaceBase implementation.
void WebGPUInterfaceStub::GenSyncTokenCHROMIUM(GLbyte* sync_token) {}
void WebGPUInterfaceStub::GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) {}
void WebGPUInterfaceStub::VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                                   GLsizei count) {}
void WebGPUInterfaceStub::WaitSyncTokenCHROMIUM(const GLbyte* sync_token) {}

// WebGPUInterface implementation
const DawnProcTable& WebGPUInterfaceStub::GetProcs() const {
  return null_procs_;
}
void WebGPUInterfaceStub::FlushCommands() {}
void WebGPUInterfaceStub::EnsureAwaitingFlush(bool* needs_flush) {}
void WebGPUInterfaceStub::FlushAwaitingCommands() {}
void WebGPUInterfaceStub::DisconnectContextAndDestroyServer() {}
ReservedTexture WebGPUInterfaceStub::ReserveTexture(WGPUDevice) {
  return {nullptr, 0, 0, 0, 0};
}
void WebGPUInterfaceStub::RequestAdapterAsync(
    PowerPreference power_preference,
    base::OnceCallback<void(int32_t, const WGPUDeviceProperties&, const char*)>
        request_adapter_callback) {}
void WebGPUInterfaceStub::RequestDeviceAsync(
    uint32_t adapter_service_id,
    const WGPUDeviceProperties& requested_device_properties,
    base::OnceCallback<void(WGPUDevice)> request_device_callback) {}

WGPUDevice WebGPUInterfaceStub::DeprecatedEnsureDefaultDeviceSync() {
  return nullptr;
}

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_interface_stub_impl_autogen.h"

}  // namespace webgpu
}  // namespace gpu

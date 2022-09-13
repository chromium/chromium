// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/webgpu_interface_stub.h"

namespace gpu {
namespace webgpu {

namespace {

class APIChannelStub : public APIChannel {
 public:
  APIChannelStub() = default;

  const DawnProcTable& GetProcs() const override { return procs_; }
  WGPUInstance GetWGPUInstance() const override { return nullptr; }
  void Disconnect() override {}

  DawnProcTable* procs() { return &procs_; }

 private:
  ~APIChannelStub() override = default;

  DawnProcTable procs_ = {};
};

}  // anonymous namespace

WebGPUInterfaceStub::WebGPUInterfaceStub()
    : api_channel_(base::MakeRefCounted<APIChannelStub>()) {}

WebGPUInterfaceStub::~WebGPUInterfaceStub() = default;

DawnProcTable* WebGPUInterfaceStub::procs() {
  return static_cast<APIChannelStub*>(api_channel_.get())->procs();
}

// InterfaceBase implementation.
void WebGPUInterfaceStub::GenSyncTokenCHROMIUM(GLbyte* sync_token) {}
void WebGPUInterfaceStub::GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) {}
void WebGPUInterfaceStub::VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                                   GLsizei count) {}
void WebGPUInterfaceStub::WaitSyncTokenCHROMIUM(const GLbyte* sync_token) {}
void WebGPUInterfaceStub::ShallowFlushCHROMIUM() {}

// WebGPUInterface implementation
scoped_refptr<APIChannel> WebGPUInterfaceStub::GetAPIChannel() const {
  return api_channel_;
}
void WebGPUInterfaceStub::FlushCommands() {}
bool WebGPUInterfaceStub::EnsureAwaitingFlush() {
  return false;
}
void WebGPUInterfaceStub::FlushAwaitingCommands() {}
ReservedTexture WebGPUInterfaceStub::ReserveTexture(
    WGPUDevice,
    const WGPUTextureDescriptor*) {
  return {nullptr, 0, 0, 0, 0};
}

WGPUDevice WebGPUInterfaceStub::DeprecatedEnsureDefaultDeviceSync() {
  return nullptr;
}

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_interface_stub_impl_autogen.h"

}  // namespace webgpu
}  // namespace gpu

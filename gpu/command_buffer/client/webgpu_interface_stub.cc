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

  WGPUInstance GetWGPUInstance() const override { return nullptr; }
  void Disconnect() override {}

 private:
  ~APIChannelStub() override = default;
};

}  // anonymous namespace

WebGPUInterfaceStub::WebGPUInterfaceStub()
    : api_channel_(base::MakeRefCounted<APIChannelStub>()) {}

WebGPUInterfaceStub::~WebGPUInterfaceStub() = default;

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

void WebGPUInterfaceStub::AssociateMailbox(
    GLuint device_id,
    GLuint device_generation,
    GLuint id,
    GLuint generation,
    uint64_t usage,
    uint64_t internal_usage,
    const WGPUTextureFormat* view_formats,
    GLuint view_format_count,
    MailboxFlags flags,
    const Mailbox& mailbox) {}

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_interface_stub_impl_autogen.h"

}  // namespace webgpu
}  // namespace gpu

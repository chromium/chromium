// Copyright 2019 The Chromium Authors
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
  void ShallowFlushCHROMIUM() override;

  // WebGPUInterface implementation
  scoped_refptr<APIChannel> GetAPIChannel() const override;
  void FlushCommands() override;
  bool EnsureAwaitingFlush() override;
  void FlushAwaitingCommands() override;
  ReservedTexture ReserveTexture(
      WGPUDevice device,
      const WGPUTextureDescriptor* optionalDesc) override;

  WGPUDevice DeprecatedEnsureDefaultDeviceSync() override;

  void AssociateMailbox(GLuint device_id,
                        GLuint device_generation,
                        GLuint id,
                        GLuint generation,
                        uint64_t usage,
                        uint64_t internal_usage,
                        const WGPUTextureFormat* view_formats,
                        GLuint view_format_count,
                        MailboxFlags flags,
                        const Mailbox& mailbox) override;

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_interface_stub_autogen.h"

 private:
  scoped_refptr<APIChannel> api_channel_;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_STUB_H_

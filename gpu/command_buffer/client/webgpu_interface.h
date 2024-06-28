// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_H_

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu.h>

#include "base/functional/callback.h"
#include "base/types/cxx23_to_underlying.h"
#include "gpu/command_buffer/client/interface_base.h"
#include "gpu/command_buffer/common/webgpu_cmd_enums.h"
#include "gpu/command_buffer/common/webgpu_cmd_ids.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace gpu {

struct Mailbox;

namespace webgpu {

struct ReservedTexture {
  WGPUTexture texture;
  uint32_t id;
  uint32_t generation;
  uint32_t deviceId;
  uint32_t deviceGeneration;
};

// APIChannel is a RefCounted class which holds the Dawn wire client.
class APIChannel : public base::RefCounted<APIChannel> {
 public:
  // Get the WGPUInstance.
  virtual WGPUInstance GetWGPUInstance() const = 0;

  // Disconnect. All commands using the WebGPU API should become a
  // no-op and server-side resources can be freed.
  virtual void Disconnect() = 0;

 protected:
  friend class base::RefCounted<APIChannel>;
  APIChannel() = default;
  virtual ~APIChannel() = default;
};

class WebGPUInterface : public InterfaceBase {
 public:
  WebGPUInterface() = default;
  virtual ~WebGPUInterface() = default;

  // Flush all commands.
  virtual void FlushCommands() = 0;

  // Ensure the awaiting flush flag is set on the device client. Returns false
  // if a flush has already been indicated, or a flush is not needed (there may
  // be no commands to flush). Returns true if the caller should schedule a
  // flush.
  virtual bool EnsureAwaitingFlush() = 0;

  // If the awaiting flush flag is set, flushes commands. Otherwise, does
  // nothing.
  virtual void FlushAwaitingCommands() = 0;

  // Get a strong reference to the APIChannel backing the implementation.
  virtual scoped_refptr<APIChannel> GetAPIChannel() const = 0;

  virtual ReservedTexture ReserveTexture(
      WGPUDevice device,
      const WGPUTextureDescriptor* optionalDesc = nullptr) = 0;

  // Gets or creates a usable WGPUDevice synchronously. It really should not
  // be used, and the async request adapter and request device APIs should be
  // used instead.
  virtual WGPUDevice DeprecatedEnsureDefaultDeviceSync() = 0;

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_interface_autogen.h"

  // NOTE: Passing WEBGPU_MAILBOX_DISCARD is only valid if the SharedImage
  // associated with `mailbox` has been created with
  // SHARED_IMAGE_USAGE_WEBGPU_WRITE and at least one of `usage` or
  // `internal_usage` contains a usage supporting lazy clearing (CopyDst or
  // RenderAttachment).
  virtual void AssociateMailbox(GLuint device_id,
                                GLuint device_generation,
                                GLuint id,
                                GLuint generation,
                                uint64_t usage,
                                uint64_t internal_usage,
                                const WGPUTextureFormat* view_formats,
                                GLuint view_format_count,
                                MailboxFlags flags,
                                const Mailbox& mailbox) = 0;

  void AssociateMailbox(GLuint device_id,
                        GLuint device_generation,
                        GLuint id,
                        GLuint generation,
                        uint64_t usage,
                        const WGPUTextureFormat* view_formats,
                        GLuint view_format_count,
                        MailboxFlags flags,
                        const Mailbox& mailbox) {
    AssociateMailbox(device_id, device_generation, id, generation, usage, 0,
                     view_formats, view_format_count, flags, mailbox);
  }

  void AssociateMailbox(GLuint device_id,
                        GLuint device_generation,
                        GLuint id,
                        GLuint generation,
                        uint64_t usage,
                        MailboxFlags flags,
                        const Mailbox& mailbox) {
    AssociateMailbox(device_id, device_generation, id, generation, usage, 0,
                     nullptr, 0, flags, mailbox);
  }

  void AssociateMailbox(GLuint device_id,
                        GLuint device_generation,
                        GLuint id,
                        GLuint generation,
                        uint64_t usage,
                        uint64_t internal_usage,
                        MailboxFlags flags,
                        const Mailbox& mailbox) {
    AssociateMailbox(device_id, device_generation, id, generation, usage,
                     internal_usage, nullptr, 0, flags, mailbox);
  }

  void AssociateMailbox(GLuint device_id,
                        GLuint device_generation,
                        GLuint id,
                        GLuint generation,
                        uint64_t usage,
                        const Mailbox& mailbox) {
    AssociateMailbox(device_id, device_generation, id, generation, usage, 0,
                     nullptr, 0, WEBGPU_MAILBOX_NONE, mailbox);
  }

  void SetWebGPUExecutionContextToken(
      const blink::WebGPUExecutionContextToken& token) {
    uint64_t high = token.value().GetHighForSerialization();
    uint64_t low = token.value().GetLowForSerialization();
    SetWebGPUExecutionContextToken(base::to_underlying(token.variant_index()),
                                   high >> 32, high & 0xFFFFFFFF, low >> 32,
                                   low & 0xFFFFFFFF);
  }
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_MAILBOX_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_MAILBOX_BUFFER_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_cpp.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class DawnControlClientHolder;

class PLATFORM_EXPORT WebGPUMailboxBuffer
    : public RefCounted<WebGPUMailboxBuffer> {
 public:
  static scoped_refptr<WebGPUMailboxBuffer> FromExistingSharedImage(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      const wgpu::Device& device,
      const wgpu::BufferDescriptor& desc,
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      const gpu::SyncToken& sync_token,
      base::OnceCallback<void(const gpu::SyncToken&)> finished_access_callback);

  ~WebGPUMailboxBuffer();

  // Dissociates this mailbox buffer from WebGPU. Returns a sync token which
  // will satisfy when the mailbox's commands have been fully processed; this
  // return value can safely be ignored if the mailbox buffer is not going to
  // be accessed further.
  gpu::SyncToken Dissociate();

  const wgpu::Buffer& GetBuffer();

 private:
  WebGPUMailboxBuffer(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      const wgpu::Device& device,
      const wgpu::BufferDescriptor& desc,
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      const gpu::SyncToken& sync_token,
      base::OnceCallback<void(const gpu::SyncToken&)> finished_access_callback);

  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  std::unique_ptr<gpu::WebGPUBufferScopedAccess> scoped_access_;
  base::OnceCallback<void(const gpu::SyncToken&)> finished_access_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_MAILBOX_BUFFER_H_

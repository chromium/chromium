// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_buffer.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_cpp.h"

namespace blink {
// static
scoped_refptr<WebGPUMailboxBuffer> WebGPUMailboxBuffer::FromExistingSharedImage(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    const wgpu::Device& device,
    const wgpu::BufferDescriptor& desc,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& sync_token,
    base::OnceCallback<void(const gpu::SyncToken&)> finished_access_callback) {
  DCHECK(dawn_control_client->GetContextProviderWeakPtr());

  return base::AdoptRef(new WebGPUMailboxBuffer(
      std::move(dawn_control_client), device, desc, std::move(shared_image),
      sync_token, std::move(finished_access_callback)));
}

WebGPUMailboxBuffer::WebGPUMailboxBuffer(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    const wgpu::Device& device,
    const wgpu::BufferDescriptor& desc,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& sync_token,
    base::OnceCallback<void(const gpu::SyncToken&)> finished_access_callback)
    : dawn_control_client_(std::move(dawn_control_client)),
      shared_image_(std::move(shared_image)),
      finished_access_callback_(std::move(finished_access_callback)) {
  DCHECK(dawn_control_client_->GetContextProviderWeakPtr());

#if BUILDFLAG(USE_DAWN)
  gpu::webgpu::WebGPUInterface* webgpu =
      dawn_control_client_->GetContextProviderWeakPtr()
          ->ContextProvider()
          .WebGPUInterface();

  uint64_t internal_usage = 0;

  scoped_access_ = shared_image_->BeginWebGPUBufferAccess(
      webgpu, sync_token, device, desc, internal_usage,
      gpu::webgpu::WEBGPU_MAILBOX_NONE);
#else
  NOTREACHED();
#endif
}

gpu::SyncToken WebGPUMailboxBuffer::Dissociate() {
  gpu::SyncToken finished_access_token;
#if BUILDFLAG(USE_DAWN)
  if (!dawn_control_client_->GetContextProviderWeakPtr()) {
    shared_image_.reset();
    return finished_access_token;
  }
  if (scoped_access_) {
    finished_access_token =
        gpu::WebGPUBufferScopedAccess::EndAccess(std::move(scoped_access_));
    if (finished_access_callback_) {
      std::move(finished_access_callback_).Run(finished_access_token);
    }
  }
  shared_image_.reset();
  return finished_access_token;
#else
  NOTREACHED();
#endif
}

const wgpu::Buffer& WebGPUMailboxBuffer::GetBuffer() {
#if BUILDFLAG(USE_DAWN)
  return scoped_access_->buffer();
#else
  NOTREACHED();
#endif
}

WebGPUMailboxBuffer::~WebGPUMailboxBuffer() {
  Dissociate();
}

}  // namespace blink

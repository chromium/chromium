// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/xr_gpu_frame_transport_delegate.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "ui/gfx/gpu_fence.h"

namespace blink {

XrGpuFrameTransportDelegate::XrGpuFrameTransportDelegate(
    XRGpuFrameTransportContext* context_provider)
    : context_provider_(context_provider) {}

XrGpuFrameTransportDelegate::~XrGpuFrameTransportDelegate() = default;

void XrGpuFrameTransportDelegate::WaitOnFence(gfx::GpuFence* fence) {
  // TODO(crbug.com/359418629): Wait on the fence like the WebGL
  // path does.
}

gpu::SyncToken XrGpuFrameTransportDelegate::GenerateSyncToken() {
  gpu::SyncToken sync_token;

  if (!context_provider_) {
    return sync_token;
  }

  auto dawn_control_client = context_provider_->GetDawnControlClient();
  if (!dawn_control_client) {
    return sync_token;
  }

  auto context_provider_weak_ptr =
      dawn_control_client->GetContextProviderWeakPtr();
  if (!context_provider_weak_ptr) {
    return sync_token;
  }

  WebGraphicsContext3DProvider& context_provider =
      context_provider_weak_ptr->ContextProvider();

  gpu::webgpu::WebGPUInterface* webgpu = context_provider.WebGPUInterface();
  TRACE_EVENT0("gpu", "GenSyncTokenCHROMIUM");
  webgpu->GenSyncTokenCHROMIUM(sync_token.GetData());

  return sync_token;
}

std::pair<gfx::GpuMemoryBufferHandle, gpu::SyncToken>
XrGpuFrameTransportDelegate::CopyImage(
    const scoped_refptr<StaticBitmapImage>& image,
    bool last_transfer_succeeded) {
  // CopyImage is only used with SUBMIT_AS_TEXTURE_HANDLE, which we don't
  // support.
  NOTREACHED();
}

void XrGpuFrameTransportDelegate::Trace(Visitor* visitor) const {
  visitor->Trace(context_provider_);
  XRFrameTransportDelegate::Trace(visitor);
}

}  // namespace blink

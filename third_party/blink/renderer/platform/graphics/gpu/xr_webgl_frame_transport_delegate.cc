// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/xr_webgl_frame_transport_delegate.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"
#include "third_party/blink/renderer/platform/graphics/image_to_buffer_copier.h"
#include "ui/gfx/gpu_fence.h"

namespace blink {

XRWebGLFrameTransportDelegate::XRWebGLFrameTransportDelegate(
    XRWebGLFrameTransportContext* context_provider)
    : context_provider_(context_provider) {}

XRWebGLFrameTransportDelegate::~XRWebGLFrameTransportDelegate() = default;

void XRWebGLFrameTransportDelegate::WaitOnFence(gfx::GpuFence* fence) {
  DVLOG(3) << "CreateClientGpuFenceCHROMIUM";
  if (!context_provider_) {
    return;
  }
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  if (!gl) {
    return;
  }
  GLuint id = gl->CreateClientGpuFenceCHROMIUM(fence->AsClientGpuFence());
  gl->WaitGpuFenceCHROMIUM(id);
  gl->DestroyGpuFenceCHROMIUM(id);
}

gpu::SyncToken XRWebGLFrameTransportDelegate::GenerateSyncToken() {
  gpu::SyncToken sync_token;
  if (!context_provider_) {
    return sync_token;
  }
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  if (!gl) {
    return sync_token;
  }

  gl->GenSyncTokenCHROMIUM(sync_token.GetData());
  return sync_token;
}

std::pair<gfx::GpuMemoryBufferHandle, gpu::SyncToken>
XRWebGLFrameTransportDelegate::CopyImage(
    const scoped_refptr<StaticBitmapImage>& image,
    bool last_transfer_succeeded) {
  if (!image_copier_ || !last_transfer_succeeded) {
    image_copier_ = std::make_unique<ImageToBufferCopier>(
        context_provider_->ContextGL(),
        context_provider_->SharedImageInterface());
  }

  auto [gpu_memory_buffer_handle, sync_token] =
      image_copier_->CopyImage(image.get());

  DrawingBuffer::Client* client = context_provider_->GetDrawingBufferClient();
  client->DrawingBufferClientRestoreTexture2DBinding();
  client->DrawingBufferClientRestoreFramebufferBinding();
  client->DrawingBufferClientRestoreRenderbufferBinding();

  return std::make_pair(std::move(gpu_memory_buffer_handle), sync_token);
}

void XRWebGLFrameTransportDelegate::Trace(Visitor* visitor) const {
  visitor->Trace(context_provider_);
  XRFrameTransportDelegate::Trace(visitor);
}

}  // namespace blink

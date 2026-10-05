// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/xr_webgl_frame_transport_delegate.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_webgl_drawing_buffer.h"
#include "third_party/blink/renderer/platform/graphics/image_to_buffer_copier.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
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

void XRWebGLFrameTransportDelegate::VerifySyncToken(
    gpu::SyncToken& sync_token) {
  if (!context_provider_) {
    return;
  }

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  if (!gl) {
    return;
  }

  int8_t* sync_token_data = sync_token.GetData();
  gl->VerifySyncTokensCHROMIUM(&sync_token_data, 1);
}

std::pair<scoped_refptr<gpu::ClientSharedImage>, gpu::SyncToken>
XRWebGLFrameTransportDelegate::CopyImage(SharedImageHolder* image,
                                         bool last_transfer_succeeded) {
  if (!image_copier_ || !last_transfer_succeeded) {
    image_copier_ = std::make_unique<ImageToBufferCopier>(
        context_provider_->ContextGL(),
        context_provider_->SharedImageInterface());
  }

  auto [copied_image, sync_token] =
      image_copier_->CopyImage(image->shared_image);
  image->sync_token = sync_token;

  DrawingBuffer::Client* client = context_provider_->GetDrawingBufferClient();
  client->DrawingBufferClientRestoreTexture2DBinding();
  client->DrawingBufferClientRestoreFramebufferBinding();
  client->DrawingBufferClientRestoreRenderbufferBinding();

  return std::make_pair(std::move(copied_image), sync_token);
}

bool XRWebGLFrameTransportDelegate::IsContextLost() {
  return !context_provider_ || !context_provider_->ContextGL();
}

void XRWebGLFrameTransportDelegate::Trace(Visitor* visitor) const {
  visitor->Trace(context_provider_);
  XRFrameTransportDelegate::Trace(visitor);
}

}  // namespace blink

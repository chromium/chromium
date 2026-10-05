// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/xr_raster_frame_transport_delegate.h"

#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"

namespace blink {

void XRRasterFrameTransportDelegate::WaitOnFence(gfx::GpuFence* fence) {}

void XRRasterFrameTransportDelegate::VerifySyncToken(
    gpu::SyncToken& sync_token) {}

std::pair<scoped_refptr<gpu::ClientSharedImage>, gpu::SyncToken>
XRRasterFrameTransportDelegate::CopyImage(SharedImageHolder* image,
                                          bool last_transfer_succeeded) {
  return {nullptr, gpu::SyncToken()};
}

bool XRRasterFrameTransportDelegate::IsContextLost() {
  auto wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!wrapper || wrapper->ContextProvider().IsContextLost()) {
    return true;
  }
  // We rely on the RasterInterface to create sync tokens.
  return !wrapper->ContextProvider().RasterInterface();
}
}  // namespace blink

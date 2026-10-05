// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_RASTER_FRAME_TRANSPORT_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_RASTER_FRAME_TRANSPORT_DELEGATE_H_

#include "third_party/blink/renderer/platform/graphics/gpu/xr_frame_transport_delegate.h"

namespace blink {

class PLATFORM_EXPORT XRRasterFrameTransportDelegate
    : public XRFrameTransportDelegate {
 public:
  XRRasterFrameTransportDelegate() = default;
  ~XRRasterFrameTransportDelegate() override = default;

  void WaitOnFence(gfx::GpuFence* fence) override;
  void VerifySyncToken(gpu::SyncToken& sync_token) override;
  std::pair<scoped_refptr<gpu::ClientSharedImage>, gpu::SyncToken> CopyImage(
      SharedImageHolder* image,
      bool last_transfer_succeeded) override;
  bool IsContextLost() override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_RASTER_FRAME_TRANSPORT_DELEGATE_H_

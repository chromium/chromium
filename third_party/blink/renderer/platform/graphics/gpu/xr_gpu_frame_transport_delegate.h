// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_GPU_FRAME_TRANSPORT_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_GPU_FRAME_TRANSPORT_DELEGATE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_cpp.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_frame_transport_delegate.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class DawnControlClientHolder;

// Unfortunately, the DawnControlClientHolder we need comes from a /modules/
// based GPUDevice, which we cannot access in this directory. By asking our
// creator to supply essentially a provider for it, we can side-step any
// potential lifespan issues from holding a reference directly to the
// DawnControlClientHolder.
class PLATFORM_EXPORT XRGpuFrameTransportContext
    : public GarbageCollectedMixin {
 public:
  virtual ~XRGpuFrameTransportContext() = default;

  virtual scoped_refptr<DawnControlClientHolder> GetDawnControlClient()
      const = 0;
};

// Due to the fact that this class needs to return gfx and gpu types, as well as
// make use of those types internally, the layering rules prohibit us from
// providing this actual implementation in /modules/xr. Thus, we provide the
// implementation here, which takes the FrameTransportContext to help with any
// dependency injection we need.
class PLATFORM_EXPORT XrGpuFrameTransportDelegate
    : public XRFrameTransportDelegate {
 public:
  explicit XrGpuFrameTransportDelegate(
      XRGpuFrameTransportContext* context_provider);
  ~XrGpuFrameTransportDelegate() override;

  // XRFrameTransportDelegate overrides
  void WaitOnFence(gfx::GpuFence* fence) override;
  gpu::SyncToken GenerateSyncToken() override;
  std::pair<gfx::GpuMemoryBufferHandle, gpu::SyncToken> CopyImage(
      const scoped_refptr<StaticBitmapImage>& image,
      bool last_transfer_succeeded) override;

  // GarbageCollected override
  void Trace(Visitor* visitor) const override;

 private:
  Member<XRGpuFrameTransportContext> context_provider_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_GPU_FRAME_TRANSPORT_DELEGATE_H_

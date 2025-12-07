// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_FRAME_TRANSPORT_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_FRAME_TRANSPORT_DELEGATE_H_

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace gfx {
class GpuFence;
}

namespace blink {

class StaticBitmapImage;

// This class exists to help serve as an abstraction for whether or not we are
// running a WebGL or WebGPU based WebXR session. The XrFrameTransport class
// in this directory can't depend on /modules/, so we have to declare the
// interface here, so that our callers (the XrFrameProvider), can supply an
// object that will do the things we need that differ based on the session type.
class PLATFORM_EXPORT XRFrameTransportDelegate
    : public GarbageCollected<XRFrameTransportDelegate> {
 public:
  virtual ~XRFrameTransportDelegate() = default;

  virtual void WaitOnFence(gfx::GpuFence* fence) = 0;
  virtual gpu::SyncToken GenerateSyncToken() = 0;
  virtual std::pair<gfx::GpuMemoryBufferHandle, gpu::SyncToken> CopyImage(
      const scoped_refptr<StaticBitmapImage>& image,
      bool last_transfer_succeeded) = 0;

  // GarbageCollected override
  virtual void Trace(Visitor* visitor) const {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_FRAME_TRANSPORT_DELEGATE_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_WEBGL_FRAME_TRANSPORT_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_WEBGL_FRAME_TRANSPORT_DELEGATE_H_

#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_frame_transport_delegate.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/gpu_fence.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
class SharedImageInterface;
}  // namespace gpu

namespace blink {

class ImageToBufferCopier;
class XRWebGLFrameTransportContext;

// Unfortunately, the various WebGL objects we need come from a /modules/
// based WebGLRenderingContextBase, which we cannot access in this directory.
// By asking our creator to supply essentially a provider for these types, we
// can side-step any potential lifespan issues from holding a reference to raw
// pointers that would be invalided if the context were lost.
class PLATFORM_EXPORT XRWebGLFrameTransportContext
    : public GarbageCollectedMixin {
 public:
  virtual ~XRWebGLFrameTransportContext() = default;

  virtual gpu::gles2::GLES2Interface* ContextGL() const = 0;
  virtual gpu::SharedImageInterface* SharedImageInterface() const = 0;
  virtual DrawingBuffer::Client* GetDrawingBufferClient() const = 0;
};

// Due to the fact that this class needs to return gfx and gpu types, as well as
// make use of those types internally, the layering rules prohibit us from
// providing this actual implementation in /modules/xr. Thus, we provide the
// implementation here, which takes the FrameTransportContext to help with any
// dependency injection we need.
class PLATFORM_EXPORT XRWebGLFrameTransportDelegate
    : public XRFrameTransportDelegate {
 public:
  explicit XRWebGLFrameTransportDelegate(
      XRWebGLFrameTransportContext* context_provider);
  ~XRWebGLFrameTransportDelegate() override;

  // XRFrameTransportDelegate overrides
  void WaitOnFence(gfx::GpuFence* fence) override;
  gpu::SyncToken GenerateSyncToken() override;
  std::pair<gfx::GpuMemoryBufferHandle, gpu::SyncToken> CopyImage(
      const scoped_refptr<StaticBitmapImage>& image,
      bool last_transfer_succeeded) override;

  // GarbageCollected override
  void Trace(Visitor* visitor) const override;

 private:
  std::unique_ptr<ImageToBufferCopier> image_copier_;
  Member<XRWebGLFrameTransportContext> context_provider_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_WEBGL_FRAME_TRANSPORT_DELEGATE_H_

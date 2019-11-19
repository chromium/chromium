// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_FRAME_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_FRAME_TRANSPORT_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace gfx {
class GpuFence;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace blink {

class GpuMemoryBufferImageCopy;
class Image;

class PLATFORM_EXPORT XRFrameTransport final
    : public GarbageCollected<XRFrameTransport>,
      public device::mojom::blink::XRPresentationClient {
 public:
  explicit XRFrameTransport();
  ~XRFrameTransport() override;

  void BindSubmitFrameClient(
      mojo::PendingReceiver<device::mojom::blink::XRPresentationClient>
          receiver);

  void PresentChange();

  void SetTransportOptions(
      device::mojom::blink::XRPresentationTransportOptionsPtr);

  bool DrawingIntoSharedBuffer();

  // Call before finalizing the frame's image snapshot.
  void FramePreImage(gpu::gles2::GLES2Interface*);

  void FrameSubmit(device::mojom::blink::XRPresentationProvider*,
                   gpu::gles2::GLES2Interface*,
                   DrawingBuffer::Client*,
                   scoped_refptr<Image> image_ref,
                   std::unique_ptr<viz::SingleReleaseCallback>,
                   int16_t vr_frame_id);

  void FrameSubmitMissing(device::mojom::blink::XRPresentationProvider*,
                          gpu::gles2::GLES2Interface*,
                          int16_t vr_frame_id);

  virtual void Trace(blink::Visitor*);

 private:
  void WaitForPreviousTransfer();
  base::TimeDelta WaitForPreviousRenderToFinish();
  base::TimeDelta WaitForGpuFenceReceived();
  void CallPreviousFrameCallback();

  // XRPresentationClient
  void OnSubmitFrameTransferred(bool success) override;
  void OnSubmitFrameRendered() override;
  void OnSubmitFrameGpuFence(const gfx::GpuFenceHandle&) override;

  mojo::Receiver<device::mojom::blink::XRPresentationClient>
      submit_frame_client_receiver_{this};

  // Used to keep the image alive until the next frame if using
  // waitForPreviousTransferToFinish.
  scoped_refptr<Image> previous_image_;
  std::unique_ptr<viz::SingleReleaseCallback> previous_image_release_callback_;

  bool waiting_for_previous_frame_transfer_ = false;
  bool last_transfer_succeeded_ = false;
  base::TimeDelta frame_wait_time_;
  bool waiting_for_previous_frame_render_ = false;
  // If using GpuFence to separate frames, need to wait for the previous
  // frame's fence, but not if this is the first frame. Separately track
  // if we're expecting a fence and the received fence itself.
  bool waiting_for_previous_frame_fence_ = false;
  std::unique_ptr<gfx::GpuFence> previous_frame_fence_;

  device::mojom::blink::XRPresentationTransportOptionsPtr transport_options_;

  std::unique_ptr<GpuMemoryBufferImageCopy> frame_copier_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_XR_FRAME_TRANSPORT_H_

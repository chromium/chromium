// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_PASS_THROUGH_IMAGE_TRANSPORT_SURFACE_H_
#define GPU_IPC_SERVICE_PASS_THROUGH_IMAGE_TRANSPORT_SURFACE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "ui/gl/gl_surface.h"

namespace gpu {

// An implementation of ImageTransportSurface that implements GLSurface through
// GLSurfaceAdapter, thereby forwarding GLSurface methods through to it.
class PassThroughImageTransportSurface : public gl::GLSurfaceAdapter {
 public:
  PassThroughImageTransportSurface(
      base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
      gl::GLSurface* surface,
      bool override_vsync_for_multi_window_swap);

  // GLSurface implementation.
  bool Initialize(gl::GLSurfaceFormat format) override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  void SwapBuffersAsync(SwapCompletionCallback completion_callback,
                        PresentationCallback presentation_callback) override;
  gfx::SwapResult SwapBuffersWithBounds(const std::vector<gfx::Rect>& rects,
                                        PresentationCallback callback) override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                PresentationCallback callback) override;
  void PostSubBufferAsync(int x,
                          int y,
                          int width,
                          int height,
                          SwapCompletionCallback completion_callback,
                          PresentationCallback presentation_callback) override;
  gfx::SwapResult CommitOverlayPlanes(PresentationCallback callback) override;
  void CommitOverlayPlanesAsync(
      SwapCompletionCallback completion_callback,
      PresentationCallback presentation_callback) override;
  void SetVSyncEnabled(bool enabled) override;

 private:
  ~PassThroughImageTransportSurface() override;

  void TrackMultiSurfaceSwap();
  void UpdateVSyncEnabled();

  void StartSwapBuffers(gfx::SwapResponse* response);
  void FinishSwapBuffers(gfx::SwapResponse response, uint64_t local_swap_id);
  void FinishSwapBuffersAsync(SwapCompletionCallback callback,
                              gfx::SwapResponse response,
                              uint64_t local_swap_id,
                              gfx::SwapResult result,
                              std::unique_ptr<gfx::GpuFence> gpu_fence);

  void BufferPresented(PresentationCallback callback,
                       uint64_t local_swap_id,
                       const gfx::PresentationFeedback& feedback);

  const bool is_gpu_vsync_disabled_;
  const bool is_multi_window_swap_vsync_override_enabled_;
  base::WeakPtr<ImageTransportSurfaceDelegate> delegate_;
  int swap_generation_ = 0;
  bool vsync_enabled_ = true;
  bool multiple_surfaces_swapped_ = false;

  // Local swap ids, which are used to make sure the swap order is correct and
  // the presentation callbacks are not called earlier than the swap ack of the
  // same swap request. Checked only when DCHECK is on.
  uint64_t local_swap_id_ = 0;

#if DCHECK_IS_ON()
  base::queue<uint64_t> pending_local_swap_ids_;
#endif

  base::WeakPtrFactory<PassThroughImageTransportSurface> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(PassThroughImageTransportSurface);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_PASS_THROUGH_IMAGE_TRANSPORT_SURFACE_H_

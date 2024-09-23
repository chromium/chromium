// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_
#define GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gfx/ca_layer_result.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/presenter.h"

// Put ui/display/mac/display_link_mac.h after ui/gl/gl_xxx.h. There is a
// conflict between macOS sdk gltypes.h and third_party/mesa_headers/GL/glext.h.
#if BUILDFLAG(IS_MAC)
#include "ui/display/mac/display_link_mac.h"
#include "ui/display/types/display_constants.h"
#endif

@class CAContext;
@class CALayer;

namespace ui {
class CALayerTreeCoordinator;
struct CARendererLayerParams;
}

namespace gpu {

class ImageTransportSurfaceOverlayMacEGL : public gl::Presenter {
 public:
  ImageTransportSurfaceOverlayMacEGL(
      DawnContextProvider* dawn_context_provider);

  // Presenter implementation
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;

  void Present(SwapCompletionCallback completion_callback,
               PresentationCallback presentation_callback,
               gfx::FrameData data) override;

  bool ScheduleOverlayPlane(
      gl::OverlayImage image,
      std::unique_ptr<gfx::GpuFence> gpu_fence,
      const gfx::OverlayPlaneData& overlay_plane_data) override;

  bool ScheduleCALayer(const ui::CARendererLayerParams& params) override;

  void SetMaxPendingSwaps(int max_pending_swaps) override;

#if BUILDFLAG(IS_MAC)
  // GLSurface override
  void SetVSyncDisplayID(int64_t display_id) override;

  void OnVSyncPresentation(ui::VSyncParamsMac params);
#endif

 private:
  ~ImageTransportSurfaceOverlayMacEGL() override;

  gfx::SwapResult SwapBuffersInternal(
      gl::GLSurface::SwapCompletionCallback completion_callback,
      gl::GLSurface::PresentationCallback presentation_callback);

  void BufferPresented(gl::GLSurface::PresentationCallback callback,
                       const gfx::PresentationFeedback& feedback);

  void CommitPresentedFrameToCA();

  std::unique_ptr<ui::CALayerTreeCoordinator> ca_layer_tree_coordinator_;

#if BUILDFLAG(IS_MAC)
  // The expected display time from CVDisplayLinkCallback for the frame being
  // committed.
  base::TimeTicks GetDisplaytime(base::TimeTicks latch_time);

  // CGDirectDisplayID of the current monitor used for Creating CVDisplayLink.
  int64_t display_id_ = display::kInvalidDisplayId;
  scoped_refptr<ui::DisplayLinkMac> display_link_mac_;
  std::unique_ptr<ui::VSyncCallbackMac> vsync_callback_mac_;

  // This is the number of vsync_callbacks running without populating CaLayer
  // parameters, used for detecting consecutive frames.
  int vsync_callback_mac_keep_alive_counter_ = 0;

  // Ensure vsync_callback_mac_ is still alive in the case of frame rate
  // throttling such as 30 fps video playback.
  // With a reduced frame rate from 60 fps to 30 fps, we skip every other
  // VSyncCallback. To prevent VSyncCallback from being turning on and off, this
  // keep_alive_counter is added.
  constexpr static int kMaxKeepAliveCounter = 8;

  // Parameters from CVDisplayLinkCallback
  base::TimeTicks current_display_time_;
  base::TimeTicks next_display_time_;
  base::TimeDelta frame_interval_;
#endif

  int cap_max_pending_swaps_ = 1;

  raw_ptr<DawnContextProvider> dawn_context_provider_ = nullptr;

  base::WeakPtrFactory<ImageTransportSurfaceOverlayMacEGL> weak_ptr_factory_;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_

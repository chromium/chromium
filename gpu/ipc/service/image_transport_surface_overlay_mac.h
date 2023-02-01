// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_
#define GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_

#include <vector>

#import "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gfx/ca_layer_result.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_switching_observer.h"
#include "ui/gl/presenter.h"

@class CAContext;
@class CALayer;

namespace ui {
class CALayerTreeCoordinator;
struct CARendererLayerParams;
}

namespace gl {
class GLFence;
}

namespace gpu {

class ImageTransportSurfaceOverlayMacEGL : public gl::Presenter,
                                           public ui::GpuSwitchingObserver {
 public:
  using VSyncCallback =
      base::RepeatingCallback<void(base::TimeTicks, base::TimeDelta)>;

  ImageTransportSurfaceOverlayMacEGL(
      gl::GLDisplayEGL* display,
      base::WeakPtr<ImageTransportSurfaceDelegate> delegate);

  // Presenter implementation
  bool Initialize(gl::GLSurfaceFormat format) override;
  void Destroy() override;
  void PrepareToDestroy(bool have_context) override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  void Present(gl::GLSurface::SwapCompletionCallback completion_callback,
               gl::GLSurface::PresentationCallback presentation_callback,
               gfx::FrameData data) override;

  // TODO(vasilyt): Remove this.
  bool SupportsCommitOverlayPlanes() override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  gl::GLSurfaceFormat GetFormat() override;
  bool OnMakeCurrent(gl::GLContext* context) override;
  bool ScheduleOverlayPlane(
      gl::OverlayImage image,
      std::unique_ptr<gfx::GpuFence> gpu_fence,
      const gfx::OverlayPlaneData& overlay_plane_data) override;
  bool ScheduleCALayer(const ui::CARendererLayerParams& params) override;

  // ui::GpuSwitchingObserver implementation.
  void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) override;

  void SetCALayerErrorCode(gfx::CALayerResult ca_layer_error_code) override;

  // GLSurface override
  bool SupportsGpuVSync() const override;
  void SetGpuVSyncEnabled(bool enabled) override;
  void SetVSyncDisplayID(int64_t display_id) override;

 private:
  ~ImageTransportSurfaceOverlayMacEGL() override;

  gfx::SwapResult SwapBuffersInternal(
      gl::GLSurface::SwapCompletionCallback completion_callback,
      gl::GLSurface::PresentationCallback presentation_callback);
  void ApplyBackpressure();
  void BufferPresented(gl::GLSurface::PresentationCallback callback,
                       const gfx::PresentationFeedback& feedback);

  base::WeakPtr<ImageTransportSurfaceDelegate> delegate_;

#if BUILDFLAG(IS_MAC)
  bool use_remote_layer_api_;
#endif
  base::scoped_nsobject<CAContext> ca_context_;
  std::unique_ptr<ui::CALayerTreeCoordinator> ca_layer_tree_coordinator_;

  gfx::Size pixel_size_;
  float scale_factor_;
  gfx::CALayerResult ca_layer_error_code_ = gfx::kCALayerSuccess;

  // A GLFence marking the end of the previous frame, used for applying
  // backpressure.
  uint64_t previous_frame_fence_ = 0;

  const VSyncCallback vsync_callback_;
  bool gpu_vsync_enabled_ = false;

  // The renderer ID that all contexts made current to this surface should be
  // targeting.
  GLint gl_renderer_id_;
  base::WeakPtrFactory<ImageTransportSurfaceOverlayMacEGL> weak_ptr_factory_;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_

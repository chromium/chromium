// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "ui/gfx/swap_result.h"
#include "ui/latency/latency_tracker.h"

namespace gfx {
struct PresentationFeedback;
}

namespace gpu {
class CommandBufferProxyImpl;
struct SwapBuffersCompleteParams;
}

namespace viz {
class ContextProviderCommandBuffer;
}

namespace content {
class ReflectorTexture;

// Adapts a WebGraphicsContext3DCommandBufferImpl into a
// viz::OutputSurface that also handles vsync parameter updates
// arriving from the GPU process.
class GpuBrowserCompositorOutputSurface
    : public BrowserCompositorOutputSurface {
 public:
  GpuBrowserCompositorOutputSurface(
      scoped_refptr<viz::ContextProviderCommandBuffer> context,
      gpu::SurfaceHandle surface_handle);

  ~GpuBrowserCompositorOutputSurface() override;

  // Called when a swap completion is sent from the GPU process.
  virtual void OnGpuSwapBuffersCompleted(
      std::vector<ui::LatencyInfo> latency_info,
      const gpu::SwapBuffersCompleteParams& params);

  // BrowserCompositorOutputSurface implementation.
  void OnReflectorChanged() override;

  // viz::OutputSurface implementation.
  void BindToClient(viz::OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SwapBuffers(viz::OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  unsigned UpdateGpuFence() override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override;
  gfx::OverlayTransform GetDisplayTransform() override;

  void SetDrawRectangle(const gfx::Rect& rect) override;

  gpu::SurfaceHandle GetSurfaceHandle() const override;

 protected:
  void OnPresentation(const gfx::PresentationFeedback& feedback);
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval);
  gpu::CommandBufferProxyImpl* GetCommandBufferProxy();

  viz::OutputSurfaceClient* client_ = nullptr;
  std::unique_ptr<ReflectorTexture> reflector_texture_;
  bool reflector_texture_defined_ = false;
  bool set_draw_rectangle_for_frame_ = false;
  // True if the draw rectangle has been set at all since the last resize.
  bool has_set_draw_rectangle_since_last_resize_ = false;
  gfx::Size size_;
  ui::LatencyTracker latency_tracker_;

 private:
  const gpu::SurfaceHandle surface_handle_;
  gfx::OverlayTransform display_transform_ = gfx::OVERLAY_TRANSFORM_NONE;
  base::WeakPtrFactory<GpuBrowserCompositorOutputSurface> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(GpuBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

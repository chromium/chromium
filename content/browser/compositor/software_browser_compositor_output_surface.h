// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SOFTWARE_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_SOFTWARE_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/common/content_export.h"
#include "ui/latency/latency_tracker.h"

namespace content {

class CONTENT_EXPORT SoftwareBrowserCompositorOutputSurface
    : public BrowserCompositorOutputSurface {
 public:
  explicit SoftwareBrowserCompositorOutputSurface(
      std::unique_ptr<viz::SoftwareOutputDevice> software_device);

  ~SoftwareBrowserCompositorOutputSurface() override;

  // OutputSurface implementation.
  void BindToClient(viz::OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SwapBuffers(viz::OutputSurfaceFrame frame) override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  unsigned UpdateGpuFence() override;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications) override;
#endif

 private:
  void SwapBuffersCallback(const std::vector<ui::LatencyInfo>& latency_info,
                           const base::TimeTicks& swap_time,
                           const gfx::Size& pixel_size);
  void UpdateVSyncCallback(const base::TimeTicks timebase,
                           const base::TimeDelta interval);

  viz::OutputSurfaceClient* client_ = nullptr;
  base::TimeDelta refresh_interval_;
  ui::LatencyTracker latency_tracker_;

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  bool needs_swap_size_notifications_ = false;
#endif

  base::WeakPtrFactory<SoftwareBrowserCompositorOutputSurface> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(SoftwareBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SOFTWARE_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

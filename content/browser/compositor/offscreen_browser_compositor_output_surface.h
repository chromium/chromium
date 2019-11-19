// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_OFFSCREEN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_OFFSCREEN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include <stdint.h>

#include <memory>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "ui/latency/latency_info.h"
#include "ui/latency/latency_tracker.h"

namespace viz {
class ContextProviderCommandBuffer;
}

namespace content {
class ReflectorTexture;

class OffscreenBrowserCompositorOutputSurface
    : public BrowserCompositorOutputSurface {
 public:
  OffscreenBrowserCompositorOutputSurface(
      scoped_refptr<viz::ContextProviderCommandBuffer> context);

  ~OffscreenBrowserCompositorOutputSurface() override;

 private:
  // viz::OutputSurface implementation.
  void BindToClient(viz::OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool alpha,
               bool stencil) override;
  void BindFramebuffer() override;
  void SwapBuffers(viz::OutputSurfaceFrame frame) override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  uint32_t GetFramebufferCopyTextureFormat() override;

  // BrowserCompositorOutputSurface implementation.
  void OnReflectorChanged() override;

  unsigned UpdateGpuFence() override;

  void OnSwapBuffersComplete(const std::vector<ui::LatencyInfo>& latency_info);

  viz::OutputSurfaceClient* client_ = nullptr;
  gfx::Size reshape_size_;
  uint32_t fbo_ = 0;
  bool reflector_changed_ = false;
  std::unique_ptr<ReflectorTexture> reflector_texture_;
  ui::LatencyTracker latency_tracker_;
  base::WeakPtrFactory<OffscreenBrowserCompositorOutputSurface>
      weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OffscreenBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_OFFSCREEN_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_DEMO_VULKAN_DEMO_H_
#define GPU_VULKAN_DEMO_VULKAN_DEMO_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "gpu/vulkan/vulkan_swap_chain.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/size.h"
#include "ui/platform_window/platform_window_base.h"
#include "ui/platform_window/platform_window_delegate.h"

class SkCanvas;
class SkSurface;

namespace base {
class RunLoop;
}

namespace viz {
class VulkanContextProvider;
}

namespace ui {
class PlatformEventSource;
}  // namespace ui

namespace gpu {

class VulkanImplementation;
class VulkanSurface;

class VulkanDemo : public ui::PlatformWindowDelegate {
 public:
  VulkanDemo();
  ~VulkanDemo() override;

  void Initialize();
  void Destroy();
  void Run();

 private:
  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override;
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {}
  void OnCloseRequest() override;
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override;
  void OnAcceleratedWidgetDestroyed() override {}
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}

  void CreateSkSurface();
  void Draw(SkCanvas* canvas, float fraction);
  void RenderFrame();

  std::unique_ptr<VulkanImplementation> vulkan_implementation_;
  scoped_refptr<viz::VulkanContextProvider> vulkan_context_provider_;
  gfx::AcceleratedWidget accelerated_widget_ = gfx::kNullAcceleratedWidget;
  std::unique_ptr<ui::PlatformEventSource> event_source_;
  std::unique_ptr<ui::PlatformWindowBase> window_;
  std::unique_ptr<VulkanSurface> vulkan_surface_;
  base::Optional<VulkanSwapChain::ScopedWrite> scoped_write_;
  sk_sp<SkSurface> sk_surface_;
  std::vector<sk_sp<SkSurface>> sk_surfaces_;
  float rotation_angle_ = 0;
  base::RunLoop* run_loop_ = nullptr;
  bool is_running_ = false;
};

}  // namespace gpu

#endif  // GPU_VULKAN_DEMO_VULKAN_DEMO_H_

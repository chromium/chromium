// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_X_VULKAN_SURFACE_X11_H_
#define GPU_VULKAN_X_VULKAN_SURFACE_X11_H_

#include <vulkan/vulkan.h>

#include "base/macros.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"

namespace x11 {
class XScopedEventSelector;
}

namespace gpu {

class VulkanSurfaceX11 : public VulkanSurface, public x11::EventObserver {
 public:
  static std::unique_ptr<VulkanSurfaceX11> Create(VkInstance vk_instance,
                                                  x11::Window parent_window);
  VulkanSurfaceX11(VkInstance vk_instance,
                   VkSurfaceKHR vk_surface,
                   x11::Window parent_window,
                   x11::Window window);
  ~VulkanSurfaceX11() override;

  // VulkanSurface:
  void Destroy() override;
  bool Reshape(const gfx::Size& size,
               gfx::OverlayTransform pre_transform) override;

 private:
  // x11::EventObserver:
  void OnEvent(const x11::Event& xevent) override;

  const x11::Window parent_window_;
  x11::Window window_;
  std::unique_ptr<x11::XScopedEventSelector> event_selector_;

  DISALLOW_COPY_AND_ASSIGN(VulkanSurfaceX11);
};

}  // namespace gpu

#endif  // GPU_VULKAN_X_VULKAN_SURFACE_X11_H_

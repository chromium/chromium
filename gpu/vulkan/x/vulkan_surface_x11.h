// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_X_VULKAN_SURFACE_X11_H_
#define GPU_VULKAN_X_VULKAN_SURFACE_X11_H_

#include <vulkan/vulkan.h>

#include "base/macros.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "ui/gfx/x/event.h"

namespace gpu {

class VulkanSurfaceX11 : public VulkanSurface {
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
  class ExposeEventForwarder;
  bool CanDispatchXEvent(const x11::Event* event);
  void ForwardXExposeEvent(const x11::Event* event);

  const x11::Window parent_window_;
  x11::Window window_;
  std::unique_ptr<ExposeEventForwarder> expose_event_forwarder_;

  DISALLOW_COPY_AND_ASSIGN(VulkanSurfaceX11);
};

}  // namespace gpu

#endif  // GPU_VULKAN_X_VULKAN_SURFACE_X11_H_

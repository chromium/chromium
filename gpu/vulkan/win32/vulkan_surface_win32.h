// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_WIN32_VULKAN_SURFACE_WIN32_H_
#define GPU_VULKAN_WIN32_VULKAN_SURFACE_WIN32_H_

#include <vulkan/vulkan_core.h>

#include "base/component_export.h"
#include "base/types/pass_key.h"
#include "gpu/vulkan/vulkan_surface.h"

namespace gfx {
class WindowImpl;
}

namespace gpu {

class COMPONENT_EXPORT(VULKAN_WIN32) VulkanSurfaceWin32 : public VulkanSurface {
 public:
  static std::unique_ptr<VulkanSurfaceWin32> Create(VkInstance vk_instance,
                                                    HWND parent_window);
  class WindowThread;
  VulkanSurfaceWin32(base::PassKey<VulkanSurfaceWin32> pass_key,
                     VkInstance vk_instance,
                     VkSurfaceKHR vk_surface,
                     scoped_refptr<WindowThread> thread,
                     std::unique_ptr<gfx::WindowImpl> window);
  ~VulkanSurfaceWin32() override;

  VulkanSurfaceWin32(const VulkanSurfaceWin32&) = delete;
  VulkanSurfaceWin32& operator=(const VulkanSurfaceWin32&) = delete;

 private:
  // VulkanSurface:
  bool Reshape(const gfx::Size& size,
               gfx::OverlayTransform pre_transform) override;

  // The thread for running message loop of child |window_|.
  // All VulkanSurfaceWin32 share one thread. The thread will be destroyed with
  // the last VulkanSurfaceWin32.
  scoped_refptr<WindowThread> thread_;
  std::unique_ptr<gfx::WindowImpl> window_;
};

}  // namespace gpu

#endif  // GPU_VULKAN_WIN32_VULKAN_SURFACE_WIN32_H_
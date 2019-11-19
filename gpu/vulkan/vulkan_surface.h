// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_SURFACE_H_
#define GPU_VULKAN_VULKAN_SURFACE_H_

#include <vulkan/vulkan.h>

#include "base/callback.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_export.h"
#include "gpu/vulkan/vulkan_swap_chain.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/swap_result.h"

namespace gpu {

class VulkanDeviceQueue;
class VulkanSwapChain;

class VULKAN_EXPORT VulkanSurface {
 public:
  // Minimum bit depth of surface.
  enum Format {
    FORMAT_RGBA_32,
    FORMAT_RGB_16,

    NUM_SURFACE_FORMATS,
    DEFAULT_SURFACE_FORMAT = FORMAT_RGBA_32
  };

  VulkanSurface(VkInstance vk_instance,
                VkSurfaceKHR surface,
                bool enforce_protected_memory);

  virtual ~VulkanSurface();

  bool Initialize(VulkanDeviceQueue* device_queue,
                  VulkanSurface::Format format);
  // Destroy() should be called when all related GPU tasks have been finished.
  void Destroy();

  gfx::SwapResult SwapBuffers();

  void Finish();

  // Reshape the the surface and recreate swap chian if it is needed. The size
  // is the current surface (window) size. The transform is the pre transform
  // relative to the hardware natural orientation, applied to frame content.
  // See VkSwapchainCreateInfoKHR::preTransform for detail.
  virtual bool Reshape(const gfx::Size& size, gfx::OverlayTransform transform);

  VulkanSwapChain* swap_chain() const { return swap_chain_.get(); }
  uint32_t swap_chain_generation() const { return swap_chain_generation_; }
  const gfx::Size& image_size() const { return image_size_; }
  gfx::OverlayTransform transform() const { return transform_; }
  VkSurfaceFormatKHR surface_format() const { return surface_format_; }

 private:
  bool CreateSwapChain(const gfx::Size& size, gfx::OverlayTransform transform);

  const VkInstance vk_instance_;

  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkSurfaceFormatKHR surface_format_ = {};
  VulkanDeviceQueue* device_queue_ = nullptr;

  const bool enforce_protected_memory_;

  // The generation of |swap_chain_|, it will be increasted if a new
  // |swap_chain_| is created due to resizing, etec.
  uint32_t swap_chain_generation_ = 0u;

  // Swap chain image size.
  gfx::Size image_size_;

  // Swap chain pre-transform.
  gfx::OverlayTransform transform_ = gfx::OVERLAY_TRANSFORM_INVALID;

  std::unique_ptr<VulkanSwapChain> swap_chain_;

  DISALLOW_COPY_AND_ASSIGN(VulkanSurface);
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_SURFACE_H_

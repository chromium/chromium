// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_surface.h"

#include <vulkan/vulkan.h>

#include <algorithm>

#include "base/macros.h"
#include "base/stl_util.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_swap_chain.h"

namespace gpu {

namespace {
const VkFormat kPreferredVkFormats32[] = {
    VK_FORMAT_B8G8R8A8_UNORM,  // FORMAT_BGRA8888,
    VK_FORMAT_R8G8B8A8_UNORM,  // FORMAT_RGBA8888,
};

const VkFormat kPreferredVkFormats16[] = {
    VK_FORMAT_R5G6B5_UNORM_PACK16,  // FORMAT_RGB565,
};

VkSurfaceTransformFlagBitsKHR ToVkSurfaceTransformFlag(
    gfx::OverlayTransform transform) {
  switch (transform) {
    case gfx::OVERLAY_TRANSFORM_NONE:
      return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      return VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR;
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR;
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
      return VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      return VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR;
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      return VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR;
    default:
      NOTREACHED() << "transform:" << transform;
      return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  };
}

gfx::OverlayTransform FromVkSurfaceTransformFlag(
    VkSurfaceTransformFlagBitsKHR transform) {
  switch (transform) {
    case VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR:
      return gfx::OVERLAY_TRANSFORM_NONE;
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR:
      return gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR:
      return gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL;
    case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
      return gfx::OVERLAY_TRANSFORM_ROTATE_90;
    case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
      return gfx::OVERLAY_TRANSFORM_ROTATE_180;
    case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
      return gfx::OVERLAY_TRANSFORM_ROTATE_270;
    default:
      NOTREACHED() << "transform:" << transform;
      return gfx::OVERLAY_TRANSFORM_INVALID;
  }
}

}  // namespace

VulkanSurface::~VulkanSurface() {
  DCHECK_EQ(static_cast<VkSurfaceKHR>(VK_NULL_HANDLE), surface_);
}

VulkanSurface::VulkanSurface(VkInstance vk_instance,
                             VkSurfaceKHR surface,
                             bool enforce_protected_memory)
    : vk_instance_(vk_instance),
      surface_(surface),
      enforce_protected_memory_(enforce_protected_memory) {
  DCHECK_NE(static_cast<VkSurfaceKHR>(VK_NULL_HANDLE), surface_);
}

bool VulkanSurface::Initialize(VulkanDeviceQueue* device_queue,
                               VulkanSurface::Format format) {
  DCHECK(format >= 0 && format < NUM_SURFACE_FORMATS);
  DCHECK(device_queue);

  device_queue_ = device_queue;

  VkResult result = VK_SUCCESS;

  VkBool32 present_support;
  if (vkGetPhysicalDeviceSurfaceSupportKHR(
          device_queue_->GetVulkanPhysicalDevice(),
          device_queue_->GetVulkanQueueIndex(), surface_,
          &present_support) != VK_SUCCESS) {
    DLOG(ERROR) << "vkGetPhysicalDeviceSurfaceSupportKHR() failed: " << result;
    return false;
  }
  if (!present_support) {
    DLOG(ERROR) << "Surface not supported by present queue.";
    return false;
  }

  // Get list of supported formats.
  uint32_t format_count = 0;
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      device_queue_->GetVulkanPhysicalDevice(), surface_, &format_count,
      nullptr);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkGetPhysicalDeviceSurfaceFormatsKHR() failed: " << result;
    return false;
  }

  std::vector<VkSurfaceFormatKHR> formats(format_count);
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      device_queue_->GetVulkanPhysicalDevice(), surface_, &format_count,
      formats.data());
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkGetPhysicalDeviceSurfaceFormatsKHR() failed: " << result;
    return false;
  }

  const VkFormat* preferred_formats = (format == FORMAT_RGBA_32)
                                          ? kPreferredVkFormats32
                                          : kPreferredVkFormats16;
  unsigned int size = (format == FORMAT_RGBA_32)
                          ? base::size(kPreferredVkFormats32)
                          : base::size(kPreferredVkFormats16);

  if (formats.size() == 1 && VK_FORMAT_UNDEFINED == formats[0].format) {
    surface_format_.format = preferred_formats[0];
    surface_format_.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  } else {
    bool format_set = false;
    for (VkSurfaceFormatKHR supported_format : formats) {
      unsigned int counter = 0;
      while (counter < size && format_set == false) {
        if (supported_format.format == preferred_formats[counter]) {
          surface_format_ = supported_format;
          surface_format_.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
          format_set = true;
        }
        counter++;
      }
      if (format_set)
        break;
    }
    if (!format_set) {
      DLOG(ERROR) << "Format not supported.";
      return false;
    }
  }
  return CreateSwapChain(gfx::Size(), gfx::OVERLAY_TRANSFORM_INVALID);
}

void VulkanSurface::Destroy() {
  swap_chain_->Destroy();
  swap_chain_ = nullptr;
  vkDestroySurfaceKHR(vk_instance_, surface_, nullptr);
  surface_ = VK_NULL_HANDLE;
}

gfx::SwapResult VulkanSurface::SwapBuffers() {
  return swap_chain_->PresentBuffer();
}

void VulkanSurface::Finish() {
  vkQueueWaitIdle(device_queue_->GetVulkanQueue());
}

bool VulkanSurface::Reshape(const gfx::Size& size,
                            gfx::OverlayTransform transform) {
  return CreateSwapChain(size, transform);
}

bool VulkanSurface::CreateSwapChain(const gfx::Size& size,
                                    gfx::OverlayTransform transform) {
  // Get Surface Information.
  VkSurfaceCapabilitiesKHR surface_caps;
  VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      device_queue_->GetVulkanPhysicalDevice(), surface_, &surface_caps);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkGetPhysicalDeviceSurfaceCapabilitiesKHR() failed: "
                << result;
    return false;
  }

  auto vk_transform = transform != gfx::OVERLAY_TRANSFORM_INVALID
                          ? ToVkSurfaceTransformFlag(transform)
                          : surface_caps.currentTransform;
  DCHECK(vk_transform == (vk_transform & surface_caps.supportedTransforms));
  if (transform == gfx::OVERLAY_TRANSFORM_INVALID)
    transform = FromVkSurfaceTransformFlag(surface_caps.currentTransform);

  // For Android, the current vulkan surface size may not match the new size
  // (the current window size), in that case, we will create a swap chain with
  // the requested new size, and vulkan surface size should match the swapchain
  // images size soon.
  gfx::Size image_size = size;
  if (image_size.IsEmpty()) {
    // If width and height of the surface are 0xFFFFFFFF, it means the surface
    // size will be determined by the extent of a swapchain targeting the
    // surface. In that case, we will use the minImageExtent for the swapchain.
    const uint32_t kUndefinedExtent = 0xFFFFFFFF;
    if (surface_caps.currentExtent.width == kUndefinedExtent &&
        surface_caps.currentExtent.height == kUndefinedExtent) {
      image_size.SetSize(surface_caps.minImageExtent.width,
                         surface_caps.minImageExtent.height);
    } else {
      image_size.SetSize(surface_caps.currentExtent.width,
                         surface_caps.currentExtent.height);
    }
    if (transform == gfx::OVERLAY_TRANSFORM_ROTATE_90 ||
        transform == gfx::OVERLAY_TRANSFORM_ROTATE_270) {
      image_size.SetSize(image_size.height(), image_size.width());
    }
  }

  DCHECK_GE(static_cast<uint32_t>(image_size.width()),
            surface_caps.minImageExtent.width);
  DCHECK_GE(static_cast<uint32_t>(image_size.height()),
            surface_caps.minImageExtent.height);
  DCHECK_LE(static_cast<uint32_t>(image_size.width()),
            surface_caps.maxImageExtent.width);
  DCHECK_LE(static_cast<uint32_t>(image_size.height()),
            surface_caps.maxImageExtent.height);
  DCHECK_GT(static_cast<uint32_t>(image_size.width()), 0u);
  DCHECK_GT(static_cast<uint32_t>(image_size.height()), 0u);

  if (image_size_ == image_size && transform_ == transform)
    return true;

  image_size_ = image_size;
  transform_ = transform;

  auto swap_chain = std::make_unique<VulkanSwapChain>();

  // Create swap chain.
  uint32_t min_image_count = std::max(surface_caps.minImageCount, 3u);
  if (!swap_chain->Initialize(device_queue_, surface_, surface_format_,
                              image_size_, min_image_count, vk_transform,
                              enforce_protected_memory_,
                              std::move(swap_chain_))) {
    return false;
  }

  swap_chain_ = std::move(swap_chain);
  ++swap_chain_generation_;
  return true;
}

}  // namespace gpu

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/vulkan/vulkan_surface.h"

#include <vulkan/vulkan.h>

#include <algorithm>

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
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
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90:
      return VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180:
      return VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270:
      return VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR;
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90:
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270:
    case gfx::OVERLAY_TRANSFORM_INVALID:
      break;
  };
  NOTREACHED_IN_MIGRATION() << "transform:" << transform;
  return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
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
      return gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90;
    case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
      return gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180;
    case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
      return gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270;
    default:
      NOTREACHED_IN_MIGRATION() << "transform:" << transform;
      return gfx::OVERLAY_TRANSFORM_INVALID;
  }
}

// Minimum VkImages in a vulkan swap chain.
uint32_t kMinImageCount = 3u;

}  // namespace

VulkanSurface::~VulkanSurface() {
  DCHECK_EQ(static_cast<VkSurfaceKHR>(VK_NULL_HANDLE), surface_);
}

VulkanSurface::VulkanSurface(VkInstance vk_instance,
                             gfx::AcceleratedWidget accelerated_widget,
                             VkSurfaceKHR surface,
                             uint64_t acquire_next_image_timeout_ns,
                             std::unique_ptr<gfx::VSyncProvider> vsync_provider)
    : vk_instance_(vk_instance),
#if BUILDFLAG(IS_ANDROID)
      a_native_window_(gl::ScopedANativeWindow::Wrap(accelerated_widget)),
#endif
      accelerated_widget_(accelerated_widget),
      surface_(surface),
      acquire_next_image_timeout_ns_(acquire_next_image_timeout_ns),
      vsync_provider_(std::move(vsync_provider)) {
  DCHECK_NE(static_cast<VkSurfaceKHR>(VK_NULL_HANDLE), surface_);
  if (!vsync_provider_) {
    vsync_provider_ = std::make_unique<gfx::FixedVSyncProvider>(
        base::TimeTicks(), base::Seconds(1) / 60);
  }
}

bool VulkanSurface::Initialize(VulkanDeviceQueue* device_queue,
                               VulkanSurface::Format format) {
  DCHECK(format >= 0 && format < NUM_SURFACE_FORMATS);
  DCHECK(device_queue);

  device_queue_ = device_queue;


  VkBool32 present_support;
  VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(
      device_queue_->GetVulkanPhysicalDevice(),
      device_queue_->GetVulkanQueueIndex(), surface_, &present_support);
  if (result != VK_SUCCESS) {
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
                          ? std::size(kPreferredVkFormats32)
                          : std::size(kPreferredVkFormats16);

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

  VkSurfaceCapabilitiesKHR surface_caps;
  result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      device_queue_->GetVulkanPhysicalDevice(), surface_, &surface_caps);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkGetPhysicalDeviceSurfaceCapabilitiesKHR() failed: "
                << result;
    return false;
  }

  constexpr auto kRequiredUsageFlags =
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  constexpr auto kOptionalUsageFlags =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  if ((surface_caps.supportedUsageFlags & kRequiredUsageFlags) !=
      kRequiredUsageFlags) {
    DLOG(ERROR) << "Vulkan surface doesn't support necessary usage. "
                   "supportedUsageFlags: 0x"
                << std::hex << surface_caps.supportedUsageFlags;
  }

  image_usage_flags_ = (kRequiredUsageFlags | kOptionalUsageFlags) &
                       surface_caps.supportedUsageFlags;

  return true;
}

void VulkanSurface::Destroy() {
  if (swap_chain_) {
    swap_chain_->Destroy();
    swap_chain_ = nullptr;
  }
  vkDestroySurfaceKHR(vk_instance_, surface_, nullptr);
  surface_ = VK_NULL_HANDLE;
}

gfx::SwapResult VulkanSurface::SwapBuffers(
    PresentationCallback presentation_callback) {
  return PostSubBuffer(gfx::Rect(image_size_),
                       std::move(presentation_callback));
}

gfx::SwapResult VulkanSurface::PostSubBuffer(
    const gfx::Rect& rect,
    PresentationCallback presentation_callback) {
  auto result = swap_chain_->PostSubBuffer(rect);
  PostSubBufferCompleted({}, std::move(presentation_callback), result);
  return result;
}

void VulkanSurface::PostSubBufferAsync(
    const gfx::Rect& rect,
    VulkanSwapChain::PostSubBufferCompletionCallback completion_callback,
    PresentationCallback presentation_callback) {
  completion_callback = base::BindOnce(
      &VulkanSurface::PostSubBufferCompleted, weak_ptr_factory_.GetWeakPtr(),
      std::move(completion_callback), std::move(presentation_callback));
  swap_chain_->PostSubBufferAsync(rect, std::move(completion_callback));
}

void VulkanSurface::Finish() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  vkQueueWaitIdle(device_queue_->GetVulkanQueue());
}

bool VulkanSurface::Reshape(const gfx::Size& size,
                            gfx::OverlayTransform transform) {
  return CreateSwapChain(size, transform);
}

base::TimeDelta VulkanSurface::GetDisplayRefreshInterval() {
  DCHECK(vsync_provider_->SupportGetVSyncParametersIfAvailable());
  base::TimeTicks timestamp;
  base::TimeDelta interval;
  vsync_provider_->GetVSyncParametersIfAvailable(&timestamp, &interval);
  return interval;
}

bool VulkanSurface::CreateSwapChain(const gfx::Size& size,
                                    gfx::OverlayTransform transform) {
  // Get Surface Information.
  VkSurfaceCapabilitiesKHR surface_caps;
  VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      device_queue_->GetVulkanPhysicalDevice(), surface_, &surface_caps);
  if (VK_SUCCESS != result) {
    LOG(FATAL) << "vkGetPhysicalDeviceSurfaceCapabilitiesKHR() failed: "
               << result;
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
    if (transform == gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90 ||
        transform == gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270) {
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

  if (image_size_ == image_size && transform_ == transform &&
      swap_chain_->state() == VK_SUCCESS) {
    return true;
  }

  image_size_ = image_size;
  transform_ = transform;

  const VkCompositeAlphaFlagBitsKHR kCompositeAlphaBits[] = {
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
  };

  for (auto composite_alpha_bit : kCompositeAlphaBits) {
    if (surface_caps.supportedCompositeAlpha & composite_alpha_bit) {
      composite_alpha_ = composite_alpha_bit;
      break;
    }
  }

  auto swap_chain =
      std::make_unique<VulkanSwapChain>(acquire_next_image_timeout_ns_);
  // Create swap chain.
  auto min_image_count = std::max(surface_caps.minImageCount, kMinImageCount);
  if (!swap_chain->Initialize(device_queue_, surface_, surface_format_,
                              image_size_, min_image_count, image_usage_flags_,
                              vk_transform, composite_alpha_,
                              std::move(swap_chain_))) {
    return false;
  }

  swap_chain_ = std::move(swap_chain);
  ++swap_chain_generation_;
  return true;
}

void VulkanSurface::PostSubBufferCompleted(
    VulkanSwapChain::PostSubBufferCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    gfx::SwapResult result) {
  if (completion_callback)
    std::move(completion_callback).Run(result);

  gfx::PresentationFeedback feedback;
  if (result == gfx::SwapResult::SWAP_FAILED) {
    feedback = gfx::PresentationFeedback::Failure();
  } else {
    DCHECK(vsync_provider_->SupportGetVSyncParametersIfAvailable());
    base::TimeTicks timestamp;
    base::TimeDelta interval;
    vsync_provider_->GetVSyncParametersIfAvailable(&timestamp, &interval);
    if (timestamp.is_null())
      timestamp = base::TimeTicks::Now();
    feedback = gfx::PresentationFeedback(timestamp, interval, /*flags=*/0);
  }

  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(presentation_callback), feedback));
  } else {
    // For webview_instrumentation_test,
    // SingleThreadTaskRunner::CurrentDefaultHandle is not set, so we have to
    // call the callback directly.
    std::move(presentation_callback).Run(feedback);
  }
}

}  // namespace gpu

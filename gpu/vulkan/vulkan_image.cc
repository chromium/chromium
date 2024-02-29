// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_image.h"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <optional>

#include "base/logging.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"

namespace gpu {
namespace {

VkImageAspectFlagBits to_plane_aspect(size_t plane) {
  static const std::array<VkImageAspectFlagBits, 4> kPlaneAspects = {{
      VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT,
      VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT,
      VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT,
      VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT,
  }};
  DCHECK_LT(plane, kPlaneAspects.size());
  return kPlaneAspects[plane];
}

}  // namespace

// static
std::unique_ptr<VulkanImage> VulkanImage::Create(
    VulkanDeviceQueue* device_queue,
    const gfx::Size& size,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkImageTiling image_tiling,
    const void* extra_image_create_info,
    const void* extra_memory_allocation_info) {
  auto image = std::make_unique<VulkanImage>(base::PassKey<VulkanImage>());
  if (!image->InitializeSingleOrJointPlanes(
          device_queue, size, format, usage, flags, image_tiling,
          extra_image_create_info, extra_memory_allocation_info,
          /*requirements=*/nullptr)) {
    return nullptr;
  }
  return image;
}

// static
std::unique_ptr<VulkanImage> VulkanImage::CreateWithExternalMemory(
    VulkanDeviceQueue* device_queue,
    const gfx::Size& size,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkImageTiling image_tiling,
    const void* extra_image_create_info,
    const void* extra_memory_allocation_info) {
  auto image = std::make_unique<VulkanImage>(base::PassKey<VulkanImage>());
  if (!image->InitializeWithExternalMemory(
          device_queue, size, format, usage, flags, image_tiling,
          extra_image_create_info, extra_memory_allocation_info)) {
    return nullptr;
  }
  return image;
}

// static
std::unique_ptr<VulkanImage> VulkanImage::CreateFromGpuMemoryBufferHandle(
    VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    const gfx::Size& size,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkImageTiling image_tiling,
    uint32_t queue_family_index) {
  auto image = std::make_unique<VulkanImage>(base::PassKey<VulkanImage>());
  if (!image->InitializeFromGpuMemoryBufferHandle(
          device_queue, std::move(gmb_handle), size, format, usage, flags,
          image_tiling, queue_family_index)) {
    return nullptr;
  }
  return image;
}

// static
std::unique_ptr<VulkanImage> VulkanImage::Create(
    VulkanDeviceQueue* device_queue,
    VkImage vk_image,
    VkDeviceMemory vk_device_memory,
    const gfx::Size& size,
    VkFormat format,
    VkImageTiling image_tiling,
    VkDeviceSize device_size,
    uint32_t memory_type_index,
    std::optional<VulkanYCbCrInfo>& ycbcr_info,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags) {
  auto image = std::make_unique<VulkanImage>(base::PassKey<VulkanImage>());
  image->device_queue_ = device_queue;
  image->image_ = vk_image;
  image->memories_[0] = VulkanMemory::Create(device_queue, vk_device_memory,
                                             device_size, memory_type_index);
  image->create_info_.extent = {static_cast<uint32_t>(size.width()),
                                static_cast<uint32_t>(size.height()), 1};
  image->create_info_.format = format;
  image->create_info_.tiling = image_tiling;
  image->ycbcr_info_ = ycbcr_info;
  image->create_info_.usage = usage;
  image->create_info_.flags = flags;
  return image;
}

VulkanImage::VulkanImage(base::PassKey<VulkanImage> pass_key) {}

VulkanImage::~VulkanImage() {
  DCHECK(!device_queue_);
  DCHECK(image_ == VK_NULL_HANDLE);
#if DCHECK_IS_ON()
  for (auto& memory : memories_) {
    DCHECK(!memory);
  }
#endif
}

void VulkanImage::Destroy() {
  if (!device_queue_)
    return;

  VkDevice vk_device = device_queue_->GetVulkanDevice();
  if (image_ != VK_NULL_HANDLE) {
    vkDestroyImage(vk_device, image_, nullptr /* pAllocator */);
    image_ = VK_NULL_HANDLE;
  }

  for (auto& memory : memories_) {
    if (memory) {
      memory->Destroy();
      memory.reset();
    }
  }

  device_queue_ = nullptr;
}

bool VulkanImage::CreateVkImage(const gfx::Size& size,
                                VkFormat format,
                                VkImageUsageFlags usage,
                                VkImageCreateFlags flags,
                                VkImageTiling image_tiling,
                                const void* extra_image_create_info) {
  DCHECK(device_queue_);
  DCHECK(image_ == VK_NULL_HANDLE);

  create_info_ = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = extra_image_create_info,
      .flags = flags,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = {static_cast<uint32_t>(size.width()),
                 static_cast<uint32_t>(size.height()), 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = image_tiling,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  VkDevice vk_device = device_queue_->GetVulkanDevice();
  VkResult result = vkCreateImage(vk_device, &create_info_,
                                  nullptr /* pAllocator */, &image_);
  create_info_.pNext = nullptr;

  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkCreateImage failed result:" << result;
    return false;
  }

  return true;
}

VkMemoryRequirements VulkanImage::GetMemoryRequirements(size_t plane) {
  DCHECK(device_queue_);
  DCHECK(image_ != VK_NULL_HANDLE);
  DCHECK(plane < plane_count_);

  VkDevice vk_device = device_queue_->GetVulkanDevice();

  if (disjoint_planes_) {
    DCHECK_LT(plane, 3u);
    VkMemoryRequirements2 requirements2 = {
        VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
    VkImagePlaneMemoryRequirementsInfo plane_memory_requirements = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO,
        .pNext = nullptr,
        .planeAspect = to_plane_aspect(plane),
    };
    VkImageMemoryRequirementsInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
        .pNext = &plane_memory_requirements,
        .image = image_,
    };
    vkGetImageMemoryRequirements2(vk_device, &info, &requirements2);
    return requirements2.memoryRequirements;
  }

  DCHECK_EQ(plane, 0u);
  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(vk_device, image_, &requirements);
  return requirements;
}

bool VulkanImage::BindMemory(size_t plane,
                             std::unique_ptr<VulkanMemory> memory) {
  DCHECK(device_queue_);
  DCHECK(image_ != VK_NULL_HANDLE);
  DCHECK(plane < plane_count_);
  DCHECK(!memories_[plane]);

  VkDevice vk_device = device_queue_->GetVulkanDevice();

  if (disjoint_planes_) {
    VkBindImagePlaneMemoryInfo image_plane_info = {
        .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
        .pNext = nullptr,
        .planeAspect = to_plane_aspect(plane),
    };

    VkBindImageMemoryInfoKHR bind_info = {
        .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
        .pNext = &image_plane_info,
        .image = image_,
        .memory = memory->device_memory(),
        .memoryOffset = 0,
    };

    VkResult result = vkBindImageMemory2(vk_device, 1, &bind_info);
    if (result != VK_SUCCESS) {
      DLOG(ERROR) << "Failed to bind memory to external VkImage plane= "
                  << plane << " :" << result;
      return false;
    }

    memories_[plane] = std::move(memory);
    return true;
  }

  DCHECK_EQ(plane, 0u);
  VkResult result = vkBindImageMemory(
      vk_device, image_, memory->device_memory(), 0 /* memoryOffset */);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "Failed to bind memory to external VkImage plane= " << plane
                << " :" << result;
    return false;
  }

  memories_[plane] = std::move(memory);
  return true;
}

bool VulkanImage::AllocateAndBindMemory(
    size_t plane,
    const VkMemoryRequirements* requirements,
    const void* extra_memory_allocation_info) {
  DCHECK(device_queue_);
  DCHECK(image_ != VK_NULL_HANDLE);

  VkMemoryRequirements tmp_requirements;
  if (!requirements) {
    tmp_requirements = GetMemoryRequirements(plane);
    if (!tmp_requirements.memoryTypeBits) {
      DLOG(ERROR) << "vkGetImageMemoryRequirements failed";
      return false;
    }
    requirements = &tmp_requirements;
  }

  // Some vulkan implementations require dedicated memory for sharing memory
  // object between vulkan instances.
  VkMemoryDedicatedAllocateInfoKHR dedicated_memory_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
      .pNext = extra_memory_allocation_info,
      .image = image_,
  };

  auto memory =
      VulkanMemory::Create(device_queue_, requirements, &dedicated_memory_info);
  if (!memory) {
    return false;
  }

  if (!BindMemory(plane, std::move(memory))) {
    return false;
  }

  return true;
}

bool VulkanImage::InitializeSingleOrJointPlanes(
    VulkanDeviceQueue* device_queue,
    const gfx::Size& size,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkImageTiling image_tiling,
    const void* extra_image_create_info,
    const void* extra_memory_allocation_info,
    const VkMemoryRequirements* requirements) {
  DCHECK(!device_queue_);
  DCHECK(image_ == VK_NULL_HANDLE);

  device_queue_ = device_queue;
  disjoint_planes_ = false;

  do {
    if (!CreateVkImage(size, format, usage, flags, image_tiling,
                       extra_image_create_info)) {
      break;
    }

    if (!AllocateAndBindMemory(0, requirements, extra_memory_allocation_info)) {
      break;
    }

    // Get subresource layout for images with VK_IMAGE_TILING_LINEAR.
    // For images with VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT, the layout is
    // initialized in InitializeWithExternalMemoryAndModifiers(). For
    // VK_IMAGE_TILING_OPTIMAL the layout is not usable and
    // vkGetImageSubresourceLayout() is illegal.
    if (image_tiling != VK_IMAGE_TILING_LINEAR) {
      return true;
    }

    const VkImageSubresource image_subresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .arrayLayer = 0,
    };
    vkGetImageSubresourceLayout(device_queue_->GetVulkanDevice(), image_,
                                &image_subresource, &layouts_[0]);

    return true;
  } while (false);

  // Initialize failed.
  Destroy();
  return false;
}

bool VulkanImage::InitializeWithExternalMemory(
    VulkanDeviceQueue* device_queue,
    const gfx::Size& size,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkImageTiling image_tiling,
    const void* extra_image_create_info,
    const void* extra_memory_allocation_info) {
#if BUILDFLAG(IS_FUCHSIA)
  constexpr auto kHandleType =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_ZIRCON_VMO_BIT_FUCHSIA;
#elif BUILDFLAG(IS_WIN)
  constexpr auto kHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
  constexpr auto kHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

  VkExternalMemoryProperties external_format_properties;
  VkResult result = QueryVkExternalMemoryProperties(
      device_queue->GetVulkanPhysicalDevice(), format, VK_IMAGE_TYPE_2D,
      image_tiling, usage, flags, kHandleType, &external_format_properties);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "External memory is not supported."
                << " format:" << format << " image_tiling:" << image_tiling
                << " usage:" << usage << " flags:" << flags;
    return false;
  }
  if (!(external_format_properties.externalMemoryFeatures &
        VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT)) {
    DLOG(ERROR) << "External memory cannot be exported."
                << " format:" << format << " image_tiling:" << image_tiling
                << " usage:" << usage << " flags:" << flags;
    return false;
  }

  handle_types_ = external_format_properties.compatibleHandleTypes;
  DCHECK(handle_types_ & kHandleType);

  VkExternalMemoryImageCreateInfoKHR external_image_create_info = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
      .pNext = extra_image_create_info,
      .handleTypes = handle_types_,
  };

  VkExportMemoryAllocateInfoKHR external_memory_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR,
      .pNext = extra_memory_allocation_info,
      .handleTypes = handle_types_,
  };

  return InitializeSingleOrJointPlanes(
      device_queue, size, format, usage, flags, image_tiling,
      &external_image_create_info, &external_memory_allocate_info,
      nullptr /* requirements */);
}

}  // namespace gpu

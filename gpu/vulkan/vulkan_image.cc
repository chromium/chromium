// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_image.h"

#include <vulkan/vulkan.h>

#include <algorithm>

#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

namespace {

base::Optional<uint32_t> FindMemoryTypeIndex(
    VkPhysicalDevice physical_device,
    const VkMemoryRequirements* requirements,
    VkMemoryPropertyFlags flags) {
  VkPhysicalDeviceMemoryProperties properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);
  constexpr uint32_t kMaxIndex = 31;
  for (uint32_t i = 0; i <= kMaxIndex; i++) {
    if (((1u << i) & requirements->memoryTypeBits) == 0)
      continue;
    if ((properties.memoryTypes[i].propertyFlags & flags) != flags)
      continue;
    return i;
  }
  NOTREACHED();
  return base::nullopt;
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
    void* vk_image_create_info_next,
    void* vk_memory_allocation_info_next) {
  auto image = std::make_unique<VulkanImage>(base::PassKey<VulkanImage>());
  if (!image->Initialize(device_queue, size, format, usage, flags, image_tiling,
                         vk_image_create_info_next,
                         vk_memory_allocation_info_next,
                         nullptr /* requirements */)) {
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
    void* image_create_info_next,
    void* memory_allocation_info_next) {
  auto image = std::make_unique<VulkanImage>(base::PassKey<VulkanImage>());
  if (!image->InitializeWithExternalMemory(
          device_queue, size, format, usage, flags, image_tiling,
          image_create_info_next, memory_allocation_info_next)) {
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
    VkImageTiling image_tiling) {
  auto image = std::make_unique<VulkanImage>(base::PassKey<VulkanImage>());
  if (!image->InitializeFromGpuMemoryBufferHandle(
          device_queue, std::move(gmb_handle), size, format, usage, flags,
          image_tiling)) {
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
    base::Optional<VulkanYCbCrInfo>& ycbcr_info,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags) {
  auto image = std::make_unique<VulkanImage>(base::PassKey<VulkanImage>());
  image->device_queue_ = device_queue;
  image->image_ = vk_image;
  image->device_memory_ = vk_device_memory;
  image->size_ = size;
  image->format_ = format;
  image->image_tiling_ = image_tiling;
  image->device_size_ = device_size;
  image->memory_type_index_ = memory_type_index;
  image->ycbcr_info_ = ycbcr_info;
  image->usage_ = usage;
  image->flags_ = flags;
  return image;
}

VulkanImage::VulkanImage(base::PassKey<VulkanImage> pass_key) {}

VulkanImage::~VulkanImage() {
  DCHECK(!device_queue_);
  DCHECK(image_ == VK_NULL_HANDLE);
  DCHECK(device_memory_ == VK_NULL_HANDLE);
}

void VulkanImage::Destroy() {
  if (!device_queue_)
    return;
  VkDevice vk_device = device_queue_->GetVulkanDevice();
  if (image_ != VK_NULL_HANDLE) {
    vkDestroyImage(vk_device, image_, nullptr /* pAllocator */);
    image_ = VK_NULL_HANDLE;
  }
  if (device_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(vk_device, device_memory_, nullptr /* pAllocator */);
    device_memory_ = VK_NULL_HANDLE;
  }
  device_queue_ = nullptr;
}

#if defined(OS_POSIX)
base::ScopedFD VulkanImage::GetMemoryFd(
    VkExternalMemoryHandleTypeFlagBits handle_type) {
  VkMemoryGetFdInfoKHR get_fd_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
      .memory = device_memory_,
      .handleType = handle_type,

  };

  VkDevice device = device_queue_->GetVulkanDevice();
  int memory_fd = -1;
  vkGetMemoryFdKHR(device, &get_fd_info, &memory_fd);
  if (memory_fd < 0) {
    DLOG(ERROR) << "Unable to extract file descriptor out of external VkImage";
    return base::ScopedFD();
  }

  return base::ScopedFD(memory_fd);
}
#endif  // defined(OS_POSIX)

bool VulkanImage::Initialize(VulkanDeviceQueue* device_queue,
                             const gfx::Size& size,
                             VkFormat format,
                             VkImageUsageFlags usage,
                             VkImageCreateFlags flags,
                             VkImageTiling image_tiling,
                             void* vk_image_create_info_next,
                             void* vk_memory_allocation_info_next,
                             const VkMemoryRequirements* requirements) {
  DCHECK(!device_queue_);
  DCHECK(image_ == VK_NULL_HANDLE);
  DCHECK(device_memory_ == VK_NULL_HANDLE);

  device_queue_ = device_queue;
  size_ = size;
  format_ = format;
  usage_ = usage;
  flags_ = flags;
  image_tiling_ = image_tiling;

  VkImageCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = vk_image_create_info_next,
      .flags = flags_,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format_,
      .extent = {size.width(), size.height(), 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = image_tiling_,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = image_layout_,
  };
  VkDevice vk_device = device_queue->GetVulkanDevice();
  VkResult result =
      vkCreateImage(vk_device, &create_info, nullptr /* pAllocator */, &image_);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkCreateImage failed result:" << result;
    device_queue_ = nullptr;
    return false;
  }

  VkMemoryRequirements tmp_requirements;
  if (!requirements) {
    vkGetImageMemoryRequirements(vk_device, image_, &tmp_requirements);
    if (!tmp_requirements.memoryTypeBits) {
      DLOG(ERROR) << "vkGetImageMemoryRequirements failed";
      Destroy();
      return false;
    }
    requirements = &tmp_requirements;
  }

  device_size_ = requirements->size;

  // Some vulkan implementations require dedicated memory for sharing memory
  // object between vulkan instances.
  VkMemoryDedicatedAllocateInfoKHR dedicated_memory_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
      .pNext = vk_memory_allocation_info_next,
      .image = image_,
  };

  auto index =
      FindMemoryTypeIndex(device_queue->GetVulkanPhysicalDevice(), requirements,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (!index) {
    DLOG(ERROR) << "Cannot find validate memory type index.";
    Destroy();
    return false;
  }

  memory_type_index_ = index.value();
  VkMemoryAllocateInfo memory_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &dedicated_memory_info,
      .allocationSize = device_size_,
      .memoryTypeIndex = memory_type_index_,
  };

  result = vkAllocateMemory(vk_device, &memory_allocate_info,
                            nullptr /* pAllocator */, &device_memory_);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkAllocateMemory failed result:" << result;
    Destroy();
    return false;
  }

  result = vkBindImageMemory(vk_device, image_, device_memory_,
                             0 /* memoryOffset */);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "Failed to bind memory to external VkImage: " << result;
    Destroy();
    return false;
  }

  // Get subresource layout for images with VK_IMAGE_TILING_LINEAR.
  // For images with VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT, the layout is
  // initialized in InitializeWithExternalMemoryAndModifiers(). For
  // VK_IMAGE_TILING_OPTIMAL the layout is not usable and
  // vkGetImageSubresourceLayout() is illegal.
  if (image_tiling_ != VK_IMAGE_TILING_LINEAR)
    return true;

  const VkImageSubresource image_subresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .arrayLayer = 0,
  };
  vkGetImageSubresourceLayout(device_queue_->GetVulkanDevice(), image_,
                              &image_subresource, &layouts_[0]);

  return true;
}

bool VulkanImage::InitializeWithExternalMemory(
    VulkanDeviceQueue* device_queue,
    const gfx::Size& size,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkImageTiling image_tiling,
    void* image_create_info_next,
    void* memory_allocation_info_next) {
#if defined(OS_FUCHSIA)
  constexpr auto kHandleType =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_TEMP_ZIRCON_VMO_BIT_FUCHSIA;
#elif defined(OS_WIN)
  constexpr auto kHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
  constexpr auto kHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

  VkPhysicalDeviceImageFormatInfo2 format_info_2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
      .format = format,
      .type = VK_IMAGE_TYPE_2D,
      .tiling = image_tiling,
      .usage = usage,
      .flags = flags,
  };

  VkPhysicalDeviceExternalImageFormatInfo external_info = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
      .handleType = kHandleType,
  };
  format_info_2.pNext = &external_info;

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifier_info = {
      .sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  // If image_tiling is VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT, a modifier_info
  // struct has to be appended.
  if (image_tiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)
    external_info.pNext = &modifier_info;
#endif

  VkImageFormatProperties2 image_format_properties_2 = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
  };
  VkExternalImageFormatProperties external_image_format_properties = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
  };
  image_format_properties_2.pNext = &external_image_format_properties;

  auto result = vkGetPhysicalDeviceImageFormatProperties2(
      device_queue->GetVulkanPhysicalDevice(), &format_info_2,
      &image_format_properties_2);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "External memory is not supported."
                << " format:" << format << " image_tiling:" << image_tiling
                << " usage:" << usage << " flags:" << flags;
    return false;
  }

  const auto& external_format_properties =
      external_image_format_properties.externalMemoryProperties;
  if (!(external_format_properties.externalMemoryFeatures &
        VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT)) {
    DLOG(ERROR) << "External memroy cannot be exported."
                << " format:" << format << " image_tiling:" << image_tiling
                << " usage:" << usage << " flags:" << flags;
    return false;
  }

  handle_types_ = external_format_properties.compatibleHandleTypes;
  DCHECK(handle_types_ & kHandleType);

  VkExternalMemoryImageCreateInfoKHR external_image_create_info = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
      .pNext = image_create_info_next,
      .handleTypes = handle_types_,
  };

  VkExportMemoryAllocateInfoKHR external_memory_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR,
      .pNext = memory_allocation_info_next,
      .handleTypes = handle_types_,
  };

  return Initialize(device_queue, size, format, usage, flags, image_tiling,
                    &external_image_create_info, &external_memory_allocate_info,
                    nullptr /* requirements */);
}

}  // namespace gpu

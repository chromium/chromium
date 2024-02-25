// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_image.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

namespace {
BASE_FEATURE(kLimitVkImageUsageToFormatFeaturesForAHB,
             "LimitVkImageUsageToFormatFeaturesForAHB",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsSinglePlaneRGBVulkanAHBFormat(VkFormat format) {
  switch (format) {
    // AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM
    case VK_FORMAT_R8G8B8A8_UNORM:
    // AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM
    case VK_FORMAT_R8G8B8_UNORM:
    // AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    // AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    // AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
      return true;
    default:
      return false;
  }
}

VkImageUsageFlags AHBUsageToImageUsage(uint64_t ahb_usage) {
  VkImageUsageFlags usage_flags = 0;

  // Get Vulkan Image usage flag equivalence of AHB usage.
  if (ahb_usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) {
    usage_flags |=
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  }
  if (ahb_usage & AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT) {
    usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  // All AHB support these usages when imported into vulkan.
  usage_flags |=
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  return usage_flags;
}

VkImageUsageFlags VkFormatFeaturesToImageUsage(VkFormatFeatureFlags features) {
  VkImageUsageFlags usage_flags = 0;

  if (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
    usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }

  if (features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) {
    usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  }

  if (features & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) {
    usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  if (features & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) {
    usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }

  return usage_flags;
}

}  // namespace

bool VulkanImage::InitializeFromGpuMemoryBufferHandle(
    VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    const gfx::Size& size,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkImageTiling image_tiling,
    uint32_t queue_family_index) {
  if (gmb_handle.type != gfx::GpuMemoryBufferType::ANDROID_HARDWARE_BUFFER) {
    DLOG(ERROR) << "gmb_handle.type is not supported. type:" << gmb_handle.type;
    return false;
  }
  DCHECK(gmb_handle.android_hardware_buffer.is_valid());
  SCOPED_CRASH_KEY_BOOL("vulkan", "gmb_buffer.is_valid",
                        gmb_handle.android_hardware_buffer.is_valid());
  auto& ahb_handle = gmb_handle.android_hardware_buffer;

  // To obtain format properties of an Android hardware buffer, include an
  // instance of VkAndroidHardwareBufferFormatPropertiesANDROID in the pNext
  // chain of the VkAndroidHardwareBufferPropertiesANDROID instance passed to
  // vkGetAndroidHardwareBufferPropertiesANDROID.
  VkAndroidHardwareBufferFormatPropertiesANDROID ahb_format_props = {
      VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID,
  };
  VkAndroidHardwareBufferPropertiesANDROID ahb_props = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
      .pNext = &ahb_format_props,
  };

  VkDevice vk_device = device_queue->GetVulkanDevice();
  VkResult result = vkGetAndroidHardwareBufferPropertiesANDROID(
      vk_device, ahb_handle.get(), &ahb_props);
  if (result != VK_SUCCESS) {
    LOG(ERROR)
        << "GetAhbProps: vkGetAndroidHardwareBufferPropertiesANDROID failed : "
        << result;
    return false;
  }

  // To create an image with an external format, include an instance of
  // VkExternalFormatANDROID in the pNext chain of VkImageCreateInfo.
  VkExternalFormatANDROID external_format = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
      // If externalFormat is zero, the effect is as if the
      // VkExternalFormatANDROID structure was not present. Otherwise, the image
      // will have the specified external format.
      .externalFormat = 0,
  };

  const bool should_use_external_format =
      !IsSinglePlaneRGBVulkanAHBFormat(ahb_format_props.format);

  if (should_use_external_format) {
    // externalFormat must be 0 or a value returned in the externalFormat member
    // of VkAndroidHardwareBufferFormatPropertiesANDROID by an earlier call to
    // vkGetAndroidHardwareBufferPropertiesANDROID.
    external_format.externalFormat = ahb_format_props.externalFormat;
  }

  // To define a set of external memory handle types that may be used as backing
  // store for an image, add a VkExternalMemoryImageCreateInfo structure to the
  // pNext chain of the VkImageCreateInfo structure.
  VkExternalMemoryImageCreateInfo external_memory_image_info = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
      .pNext = &external_format,
      .handleTypes =
          VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
  };

  // Get the AHB description.
  AHardwareBuffer_Desc ahb_desc = {};
  base::AndroidHardwareBufferCompat::GetInstance().Describe(ahb_handle.get(),
                                                            &ahb_desc);

  // Get Vulkan Image usage flag equivalence of AHB usage.
  VkImageUsageFlags usage_flags = AHBUsageToImageUsage(ahb_desc.usage);

  if (!(usage_flags &
        (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))) {
    LOG(ERROR) << "No valid usage flags found";
    return false;
  }

  // If we're using external format, we should limit our usage to supported
  // format features.
  if (should_use_external_format &&
      base::FeatureList::IsEnabled(kLimitVkImageUsageToFormatFeaturesForAHB)) {
    usage_flags &=
        VkFormatFeaturesToImageUsage(ahb_format_props.formatFeatures);
  }

  VkImageCreateFlags create_flags = 0;
  if (ahb_desc.usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT) {
    create_flags = VK_IMAGE_CREATE_PROTECTED_BIT;
  }

  // To import memory created outside of the current Vulkan instance from an
  // Android hardware buffer, add a VkImportAndroidHardwareBufferInfoANDROID
  // structure to the pNext chain of the VkMemoryAllocateInfo structure.
  VkImportAndroidHardwareBufferInfoANDROID ahb_import_info = {
      .sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
      .buffer = ahb_handle.get(),
  };

  VkMemoryRequirements requirements = {
      .size = ahb_props.allocationSize,
      .memoryTypeBits = ahb_props.memoryTypeBits,
  };

  if (!InitializeSingleOrJointPlanes(
          device_queue, gfx::Size(ahb_desc.width, ahb_desc.height),
          should_use_external_format ? VK_FORMAT_UNDEFINED
                                     : ahb_format_props.format,
          usage_flags, create_flags, VK_IMAGE_TILING_OPTIMAL,
          &external_memory_image_info, &ahb_import_info, &requirements)) {
    return false;
  }

  queue_family_index_ = queue_family_index;

  if (should_use_external_format) {
    ycbcr_info_.emplace(VK_FORMAT_UNDEFINED, ahb_format_props.externalFormat,
                        ahb_format_props.suggestedYcbcrModel,
                        ahb_format_props.suggestedYcbcrRange,
                        ahb_format_props.suggestedXChromaOffset,
                        ahb_format_props.suggestedYChromaOffset,
                        ahb_format_props.formatFeatures);
  }

  return true;
}

}  // namespace gpu

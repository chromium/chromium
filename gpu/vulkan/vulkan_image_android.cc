// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_image.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/logging.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

bool VulkanImage::InitializeFromGpuMemoryBufferHandle(
    VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    const gfx::Size& size,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkImageTiling image_tiling) {
  if (gmb_handle.type != gfx::GpuMemoryBufferType::ANDROID_HARDWARE_BUFFER) {
    DLOG(ERROR) << "gmb_handle.type is not supported. type:" << gmb_handle.type;
    return false;
  }
  DCHECK(gmb_handle.android_hardware_buffer.is_valid());
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

  // If image has an external format, format must be VK_FORMAT_UNDEFINED.
  if (ahb_format_props.format == VK_FORMAT_UNDEFINED) {
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

  // Intended usage of the image.
  VkImageUsageFlags usage_flags = 0;
  // Get Vulkan Image usage flag equivalence of AHB usage.
  if (ahb_desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) {
    usage_flags = usage_flags | VK_IMAGE_USAGE_SAMPLED_BIT |
                  VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  }
  if (ahb_desc.usage & AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT) {
    usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  // TODO(vikassoni) : AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP is supported from API
  // level 28 which is not part of current android_ndk version in chromium. Add
  // equivalent VK usage later.
  if (!usage_flags) {
    LOG(ERROR) << "No valid usage flags found";
    return false;
  }

  // Skia currently requires all wrapped VkImages to have transfer src and dst
  // usage. Additionally all AHB support these usages when imported into vulkan.
  usage_flags |=
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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
  if (!Initialize(device_queue, size, ahb_format_props.format, usage_flags,
                  create_flags, VK_IMAGE_TILING_OPTIMAL,
                  &external_memory_image_info, &ahb_import_info,
                  &requirements)) {
    return false;
  }

  // VkImage is imported from external.
  queue_family_index_ = VK_QUEUE_FAMILY_EXTERNAL;

  if (ahb_format_props.format == VK_FORMAT_UNDEFINED) {
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

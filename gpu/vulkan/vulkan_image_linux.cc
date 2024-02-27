// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_image.h"

#include <tuple>
#include <vector>

#include "base/logging.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"

namespace gpu {

//  static
std::unique_ptr<VulkanImage> VulkanImage::CreateWithExternalMemoryAndModifiers(
    VulkanDeviceQueue* device_queue,
    const gfx::Size& size,
    VkFormat format,
    std::vector<uint64_t> modifiers,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags) {
  auto image = std::make_unique<VulkanImage>(base::PassKey<VulkanImage>());
  if (!image->InitializeWithExternalMemoryAndModifiers(
          device_queue, size, format, std::move(modifiers), usage, flags)) {
    return nullptr;
  }
  return image;
}

bool VulkanImage::InitializeFromGpuMemoryBufferHandle(
    VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    const gfx::Size& size,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkImageTiling image_tiling,
    uint32_t queue_family_index) {
  if (gmb_handle.type != gfx::GpuMemoryBufferType::NATIVE_PIXMAP) {
    DLOG(ERROR) << "GpuMemoryBuffer is not supported. type:" << gmb_handle.type;
    return false;
  }

  queue_family_index_ = queue_family_index;
  auto& native_pixmap_handle = gmb_handle.native_pixmap_handle;

  auto& scoped_fd = native_pixmap_handle.planes[0].fd;
  if (!scoped_fd.is_valid()) {
    DLOG(ERROR) << "GpuMemoryBufferHandle doesn't have a valid fd.";
    return false;
  }

  bool using_modifier =
      native_pixmap_handle.modifier != gfx::NativePixmapHandle::kNoModifier &&
      gfx::HasExtension(device_queue->enabled_extensions(),
                        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);

  VkExternalMemoryImageCreateInfoKHR external_image_create_info = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
      .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
  };

  std::vector<VkSubresourceLayout> planeLayouts(
      native_pixmap_handle.planes.size());
  for (size_t i = 0; i < native_pixmap_handle.planes.size(); ++i) {
    planeLayouts[i].offset = native_pixmap_handle.planes[i].offset;
    planeLayouts[i].size = 0;
    planeLayouts[i].rowPitch = native_pixmap_handle.planes[i].stride;
    planeLayouts[i].arrayPitch = 0;
    planeLayouts[i].depthPitch = 0;
  }

  VkImageDrmFormatModifierExplicitCreateInfoEXT modifier_info = {
      .sType =
          VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
      .drmFormatModifier = native_pixmap_handle.modifier,
      .drmFormatModifierPlaneCount =
          static_cast<uint32_t>(native_pixmap_handle.planes.size()),
      .pPlaneLayouts = planeLayouts.data(),
  };

  if (using_modifier) {
    DCHECK_EQ(image_tiling, VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT);
    external_image_create_info.pNext = &modifier_info;
  }

  int memory_fd = scoped_fd.release();
  VkImportMemoryFdInfoKHR import_memory_fd_info = {
      .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
      .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
      .fd = memory_fd,
  };

  VkExportMemoryAllocateInfo export_memory_info = {
      VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR};
  VkExternalMemoryProperties external_memory_properties;
  VkResult vk_result = QueryVkExternalMemoryProperties(
      device_queue->GetVulkanPhysicalDevice(), format, VK_IMAGE_TYPE_2D,
      image_tiling, usage, flags,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
      &external_memory_properties);
  if (vk_result == VK_SUCCESS) {
    export_memory_info.handleTypes =
        external_memory_properties.compatibleHandleTypes;

    import_memory_fd_info.pNext = &export_memory_info;
  }

  VkMemoryRequirements* requirements = nullptr;
  // TODO support multiple plane
  bool result = InitializeSingleOrJointPlanes(
      device_queue, size, format, usage, flags, image_tiling,
      &external_image_create_info, &import_memory_fd_info, requirements);
  // If Initialize successfully, the fd in scoped_fd should be owned by vulkan,
  // otherwise take the ownership of the fd back.
  if (!result) {
    scoped_fd.reset(memory_fd);
  }

  return result;
}

bool VulkanImage::InitializeWithExternalMemoryAndModifiers(
    VulkanDeviceQueue* device_queue,
    const gfx::Size& size,
    VkFormat format,
    std::vector<uint64_t> modifiers,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags) {
  DCHECK(gfx::HasExtension(device_queue->enabled_extensions(),
                           VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME));
  DCHECK(!modifiers.empty());
  VkPhysicalDevice physical_device = device_queue->GetVulkanPhysicalDevice();

  // Query all supported format modifiers.
  VkDrmFormatModifierPropertiesListEXT modifier_props_list = {
      .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
  };
  VkFormatProperties2 format_props = {
      .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
      .pNext = &modifier_props_list,
  };
  vkGetPhysicalDeviceFormatProperties2(physical_device, format, &format_props);

  std::vector<VkDrmFormatModifierPropertiesEXT> props_vector;
  props_vector.resize(modifier_props_list.drmFormatModifierCount);
  modifier_props_list.pDrmFormatModifierProperties = props_vector.data();
  vkGetPhysicalDeviceFormatProperties2(physical_device, format, &format_props);

  // Call GetImageFormatProperties with every modifier and filter the list
  // down to those that we know work.
  std::erase_if(props_vector, [&](const VkDrmFormatModifierPropertiesEXT& p) {
    VkPhysicalDeviceImageDrmFormatModifierInfoEXT mod_info = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
        .drmFormatModifier = p.drmFormatModifier,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkPhysicalDeviceImageFormatInfo2 format_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .pNext = &mod_info,
        .format = format,
        .type = VK_IMAGE_TYPE_2D,
        .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
        .usage = usage,
        .flags = flags,
    };
    VkImageFormatProperties2 format_props = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
    };
    auto result = vkGetPhysicalDeviceImageFormatProperties2(
        physical_device, &format_info, &format_props);
    return result != VK_SUCCESS;
  });
  if (props_vector.empty())
    return false;

  // Find compatible modifiers.
  std::erase_if(modifiers, [&props_vector](uint64_t modifier) {
    for (const auto& modifier_props : props_vector) {
      if (modifier == modifier_props.drmFormatModifier)
        return false;
    }
    return true;
  });
  if (modifiers.empty())
    return false;

  VkImageDrmFormatModifierListCreateInfoEXT modifier_list = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT,
      .drmFormatModifierCount = static_cast<uint32_t>(modifiers.size()),
      .pDrmFormatModifiers = modifiers.data(),
  };

  if (!InitializeWithExternalMemory(device_queue, size, format, usage, flags,
                                    VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
                                    &modifier_list,
                                    /*extra_memory_allocation_info=*/nullptr)) {
    return false;
  }

  // Vulkan implementation will select a modifier from |modifiers|, so we need
  // to query it from the VkImage.
  VkImageDrmFormatModifierPropertiesEXT image_modifier_props = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT,
  };
  auto result = vkGetImageDrmFormatModifierPropertiesEXT(
      device_queue->GetVulkanDevice(), image_, &image_modifier_props);
  DCHECK_EQ(result, VK_SUCCESS);

  modifier_ = image_modifier_props.drmFormatModifier;
  for (auto props : props_vector) {
    if (props.drmFormatModifier == modifier_) {
      plane_count_ = props.drmFormatModifierPlaneCount;
      break;
    }
  }
  DCHECK_GE(plane_count_, 1u);
  DCHECK_LE(plane_count_, 3u);

  for (uint32_t i = 0; i < plane_count_; i++) {
    // Based on spec VK_IMAGE_ASPECT_MEMORY_PLANE_i_BIT_EXT should be used for
    // VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT tiling. But we have to use
    // VK_IMAGE_ASPECT_PLANE_i_BIT because mesa only handles
    // VK_IMAGE_ASPECT_PLANE_i_BIT.
    // TODO(penghuang): use VK_IMAGE_ASPECT_MEMORY_PLANE_i_BIT_EXT when the mesa
    // can handle VK_IMAGE_ASPECT_MEMORY_PLANE_i_BIT_EXT.
    const VkImageSubresource image_subresource = {
        .aspectMask =
            static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_PLANE_0_BIT << i),
        .mipLevel = 0,
        .arrayLayer = 0,
    };
    vkGetImageSubresourceLayout(device_queue->GetVulkanDevice(), image_,
                                &image_subresource, &layouts_[i]);
  }

  return true;
}

}  // namespace gpu

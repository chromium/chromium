// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/drm_modifiers_filter_vulkan.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "ui/gfx/buffer_format_util.h"

namespace gpu {

DrmModifiersFilterVulkan::DrmModifiersFilterVulkan(
    raw_ptr<gpu::VulkanImplementation> vulkan_implementation)
    : vulkan_implementation_(vulkan_implementation) {}

DrmModifiersFilterVulkan::~DrmModifiersFilterVulkan() = default;

std::vector<uint64_t> DrmModifiersFilterVulkan::Filter(
    gfx::BufferFormat format,
    const std::vector<uint64_t>& modifiers) {
  VkFormat vulkan_format = ToVkFormat(format);
  gpu::VulkanInstance* instance = vulkan_implementation_->GetVulkanInstance();
  CHECK(instance->vulkan_info().physical_devices.size() > 0);
  VkPhysicalDevice phys_dev =
      instance->vulkan_info().physical_devices.front().device;
  gpu::VulkanFunctionPointers* ptrs = gpu::GetVulkanFunctionPointers();
  VkDrmFormatModifierPropertiesListEXT format_modifier_properties_list = {
      .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
      .drmFormatModifierCount = 0,
      .pDrmFormatModifierProperties = nullptr,
  };
  VkFormatProperties2 format_props = {
      .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
      .pNext = &format_modifier_properties_list,
  };
  ptrs->vkGetPhysicalDeviceFormatProperties2(phys_dev, vulkan_format,
                                             &format_props);
  uint32_t modifier_count =
      format_modifier_properties_list.drmFormatModifierCount;
  VkDrmFormatModifierPropertiesEXT format_modifier_properties[modifier_count];
  format_modifier_properties_list.pDrmFormatModifierProperties =
      format_modifier_properties;
  ptrs->vkGetPhysicalDeviceFormatProperties2(phys_dev, vulkan_format,
                                             &format_props);
  base::flat_set<uint64_t> vulkan_modifiers;
  for (size_t i = 0; i < modifier_count; i++) {
    uint64_t modifier = format_modifier_properties[i].drmFormatModifier;
    vulkan_modifiers.insert(modifier);
  }
  std::vector<uint64_t> intersection;
  for (const auto& modifier : modifiers) {
    if (base::Contains(vulkan_modifiers, modifier)) {
      intersection.push_back(modifier);
    }
  }
  return intersection;
}

}  // namespace gpu

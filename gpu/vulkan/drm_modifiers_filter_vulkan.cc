// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/drm_modifiers_filter_vulkan.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_util.h"
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
  std::vector<VkDrmFormatModifierPropertiesEXT> modifier_props =
      QueryVkDrmFormatModifierPropertiesEXT(phys_dev, vulkan_format);

  base::flat_set<uint64_t> vulkan_modifiers;
  for (const auto& props : modifier_props) {
    vulkan_modifiers.insert(props.drmFormatModifier);
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

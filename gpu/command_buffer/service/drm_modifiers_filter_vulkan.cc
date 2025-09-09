// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/drm_modifiers_filter_vulkan.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/gfx/linux/drm_util_linux.h"  //nogncheck
#endif

namespace {

VkFormat ToTextureVkFormat(viz::SharedImageFormat format) {
  VkFormat vulkan_format;
  if (format.is_single_plane()) {
    vulkan_format = gpu::ToVkFormatSinglePlanar(format);
  } else {
    // Format prefers external sampler.
    format.SetPrefersExternalSampler();
    vulkan_format = gpu::ToVkFormatExternalSampler(format);
  }
  return vulkan_format;
}

}  // anonymous namespace

namespace gpu {

void PopulateVkDrmFormatsAndModifiers(
    VulkanDeviceQueue* device_queue,
    base::flat_map<uint32_t, std::vector<uint64_t>>&
        drm_formats_and_modifiers) {
#if BUILDFLAG(IS_CHROMEOS)
  for (int i = 0; i <= static_cast<int>(gfx::BufferFormat::LAST); i++) {
    viz::SharedImageFormat si_format =
        viz::GetSharedImageFormat(static_cast<gfx::BufferFormat>(i));
    VkFormat vulkan_format = ToTextureVkFormat(si_format);
    int fourcc_format = ui::GetFourCCFormatFromSharedImageFormat(si_format);
    if (vulkan_format == VK_FORMAT_UNDEFINED || fourcc_format == 0) {
      continue;
    }

    std::vector<VkDrmFormatModifierPropertiesEXT> modifier_props =
        QueryVkDrmFormatModifierPropertiesEXT(
            device_queue->GetVulkanPhysicalDevice(), vulkan_format);
    if (modifier_props.empty()) {
      continue;
    }

    std::vector<uint64_t> modifiers;
    modifiers.reserve(modifier_props.size());
    for (const auto& props : modifier_props) {
      modifiers.push_back(props.drmFormatModifier);
    }
    drm_formats_and_modifiers.emplace(fourcc_format, std::move(modifiers));
  }
#endif
}

DrmModifiersFilterVulkan::DrmModifiersFilterVulkan(
    raw_ptr<gpu::VulkanImplementation> vulkan_implementation)
    : vulkan_implementation_(vulkan_implementation) {}

DrmModifiersFilterVulkan::~DrmModifiersFilterVulkan() = default;

std::vector<uint64_t> DrmModifiersFilterVulkan::Filter(
    viz::SharedImageFormat format,
    const std::vector<uint64_t>& modifiers) {
  CHECK(viz::HasEquivalentBufferFormat(format));
  VkFormat vulkan_format = ToTextureVkFormat(format);
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

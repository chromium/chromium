// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/vulkan_info.h"

#include <string_view>

#include "gpu/ipc/common/vulkan_info.mojom.h"
#include "gpu/ipc/common/vulkan_info_mojom_traits.h"

namespace gpu {

VulkanPhysicalDeviceInfo::VulkanPhysicalDeviceInfo() = default;
VulkanPhysicalDeviceInfo::VulkanPhysicalDeviceInfo(
    const VulkanPhysicalDeviceInfo& other) = default;
VulkanPhysicalDeviceInfo::~VulkanPhysicalDeviceInfo() = default;
VulkanPhysicalDeviceInfo& VulkanPhysicalDeviceInfo::operator=(
    const VulkanPhysicalDeviceInfo& info) = default;

VulkanInfo::VulkanInfo() = default;
VulkanInfo::~VulkanInfo() = default;

VulkanInfo::VulkanInfo(const VulkanInfo& other) {
  *this = other;
}

VulkanInfo& VulkanInfo::operator=(const VulkanInfo& other) {
  api_version = other.api_version;
  used_api_version = other.used_api_version;
  instance_extensions = other.instance_extensions;
  instance_layers = other.instance_layers;
  physical_devices = other.physical_devices;
  SetEnabledInstanceExtensions(other.enabled_instance_extensions);
  return *this;
}

std::vector<uint8_t> VulkanInfo::Serialize() const {
  return gpu::mojom::VulkanInfo::Serialize(this);
}

void VulkanInfo::SetEnabledInstanceExtensions(
    const std::vector<const char*>& extensions) {
  enabled_instance_extensions.clear();
  for (const auto* const extension : extensions) {
    bool found = false;
    for (const auto& instance_extension : instance_extensions) {
      if (strcmp(extension, instance_extension.extensionName) == 0) {
        enabled_instance_extensions.push_back(instance_extension.extensionName);
        found = true;
        break;
      }
    }
    if (!found) {
      LOG(ERROR) << "The enabled extension '" << extension
                 << "' is not in instance_extensions!";
    }
  }
}

void VulkanInfo::SetEnabledInstanceExtensions(
    const std::vector<std::string_view>& extensions) {
  enabled_instance_extensions.clear();
  for (const auto& extension : extensions) {
    bool found = false;
    for (const auto& instance_extension : instance_extensions) {
      if (extension == instance_extension.extensionName) {
        enabled_instance_extensions.push_back(instance_extension.extensionName);
        found = true;
        break;
      }
    }
    if (!found) {
      LOG(ERROR) << "The enabled extension '" << extension
                 << "' is not in instance_extensions!";
    }
  }
}

}  // namespace gpu

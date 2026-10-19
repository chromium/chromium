// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_info.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"

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
  if (this == &other) {
    return *this;
  }
  api_version = other.api_version;
  used_api_version = other.used_api_version;
  instance_extensions = other.instance_extensions;
  instance_layers = other.instance_layers;
  physical_devices = other.physical_devices;
  SetEnabledInstanceExtensions(
      base::ToVector<std::string_view>(other.enabled_instance_extensions));
  return *this;
}

void VulkanInfo::SetEnabledInstanceExtensions(
    base::span<const std::string_view> extensions) {
  enabled_instance_extensions.clear();
  for (std::string_view extension : extensions) {
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

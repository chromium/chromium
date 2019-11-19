// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_VULKAN_INFO_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_VULKAN_INFO_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "gpu/config/vulkan_info.h"
#include "gpu/ipc/common/vulkan_info.mojom-shared.h"
#include "gpu/ipc/common/vulkan_types_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::VulkanPhysicalDeviceInfoDataView,
                    gpu::VulkanPhysicalDeviceInfo> {
  static const VkPhysicalDeviceProperties& properties(
      const gpu::VulkanPhysicalDeviceInfo& input) {
    return input.properties;
  }

  static const std::vector<VkLayerProperties>& layers(
      const gpu::VulkanPhysicalDeviceInfo& input) {
    return input.layers;
  }

  static const VkPhysicalDeviceFeatures& features(
      const gpu::VulkanPhysicalDeviceInfo& input) {
    return input.features;
  }

  static bool feature_sampler_ycbcr_conversion(
      const gpu::VulkanPhysicalDeviceInfo& input) {
    return input.feature_sampler_ycbcr_conversion;
  }

  static bool feature_protected_memory(
      const gpu::VulkanPhysicalDeviceInfo& input) {
    return input.feature_protected_memory;
  }

  static const std::vector<VkQueueFamilyProperties>& queue_families(
      const gpu::VulkanPhysicalDeviceInfo& input) {
    return input.queue_families;
  }

  static bool Read(gpu::mojom::VulkanPhysicalDeviceInfoDataView data,
                   gpu::VulkanPhysicalDeviceInfo* out) {
    if (!data.ReadProperties(&out->properties))
      return false;
    if (!data.ReadLayers(&out->layers))
      return false;
    if (!data.ReadFeatures(&out->features))
      return false;
    out->feature_sampler_ycbcr_conversion =
        data.feature_sampler_ycbcr_conversion();
    out->feature_protected_memory = data.feature_protected_memory();
    if (!data.ReadQueueFamilies(&out->queue_families))
      return false;
    return true;
  }
};

template <>
struct StructTraits<gpu::mojom::VulkanInfoDataView, gpu::VulkanInfo> {
  static uint32_t api_version(const gpu::VulkanInfo& input) {
    return input.api_version;
  }

  static uint32_t used_api_version(const gpu::VulkanInfo& input) {
    return input.used_api_version;
  }

  static const std::vector<VkExtensionProperties>& instance_extensions(
      const gpu::VulkanInfo& input) {
    return input.instance_extensions;
  }

  static std::vector<base::StringPiece> enabled_instance_extensions(
      const gpu::VulkanInfo& input) {
    std::vector<base::StringPiece> extensions;
    extensions.reserve(input.enabled_instance_extensions.size());
    for (const char* extension : input.enabled_instance_extensions)
      extensions.emplace_back(extension);
    return extensions;
  }

  static const std::vector<VkLayerProperties>& instance_layers(
      const gpu::VulkanInfo& input) {
    return input.instance_layers;
  }

  static const std::vector<gpu::VulkanPhysicalDeviceInfo>& physical_devices(
      const gpu::VulkanInfo& input) {
    return input.physical_devices;
  }

  static bool Read(gpu::mojom::VulkanInfoDataView data, gpu::VulkanInfo* out) {
    out->api_version = data.api_version();
    out->used_api_version = data.used_api_version();

    if (!data.ReadInstanceExtensions(&out->instance_extensions))
      return false;

    std::vector<base::StringPiece> extensions;
    if (!data.ReadEnabledInstanceExtensions(&extensions))
      return false;
    out->SetEnabledInstanceExtensions(extensions);
    return data.ReadInstanceLayers(&out->instance_layers) &&
           data.ReadPhysicalDevices(&out->physical_devices);
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_VULKAN_INFO_MOJOM_TRAITS_H_

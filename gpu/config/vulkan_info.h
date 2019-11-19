// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_VULKAN_INFO_H_
#define GPU_CONFIG_VULKAN_INFO_H_

#include <vulkan/vulkan.h>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/extension_set.h"

namespace gpu {

class GPU_EXPORT VulkanPhysicalDeviceInfo {
 public:
  VulkanPhysicalDeviceInfo();
  VulkanPhysicalDeviceInfo(const VulkanPhysicalDeviceInfo& other);
  ~VulkanPhysicalDeviceInfo();
  VulkanPhysicalDeviceInfo& operator=(const VulkanPhysicalDeviceInfo& other);

  // This is a local variable in GPU process, it will not be sent via IPC.
  VkPhysicalDevice device = VK_NULL_HANDLE;

  VkPhysicalDeviceProperties properties = {};
  std::vector<VkLayerProperties> layers;

  VkPhysicalDeviceFeatures features = {};
  // Extended physical device features:
  bool feature_sampler_ycbcr_conversion = false;
  bool feature_protected_memory = false;

  std::vector<VkQueueFamilyProperties> queue_families;
};

class GPU_EXPORT VulkanInfo {
 public:
  VulkanInfo();
  VulkanInfo(const VulkanInfo& other);
  ~VulkanInfo();
  VulkanInfo& operator=(const VulkanInfo& other);

  std::vector<uint8_t> Serialize() const;

  void SetEnabledInstanceExtensions(const std::vector<const char*>& extensions);
  void SetEnabledInstanceExtensions(
      const std::vector<base::StringPiece>& extensions);

  uint32_t api_version = VK_MAKE_VERSION(1, 0, 0);
  uint32_t used_api_version = VK_MAKE_VERSION(1, 0, 0);
  std::vector<VkExtensionProperties> instance_extensions;
  std::vector<const char*> enabled_instance_extensions;
  std::vector<VkLayerProperties> instance_layers;
  std::vector<VulkanPhysicalDeviceInfo> physical_devices;
};

}  // namespace gpu

#endif  // GPU_CONFIG_VULKAN_INFO_H_

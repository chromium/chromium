// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_DEVICE_QUEUE_H_
#define GPU_VULKAN_VULKAN_DEVICE_QUEUE_H_

#include <vulkan/vulkan.h>

#include <memory>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_export.h"
#include "ui/gfx/extension_set.h"

namespace gpu {

class VulkanCommandPool;
class VulkanFenceHelper;
class VulkanInfo;

class VULKAN_EXPORT VulkanDeviceQueue {
 public:
  enum DeviceQueueOption {
    GRAPHICS_QUEUE_FLAG = 0x01,
    PRESENTATION_SUPPORT_QUEUE_FLAG = 0x02,
  };

  VulkanDeviceQueue(VkInstance vk_instance,
                    bool enforce_protected_memory = false);
  ~VulkanDeviceQueue();

  using GetPresentationSupportCallback =
      base::RepeatingCallback<bool(VkPhysicalDevice,
                                   const std::vector<VkQueueFamilyProperties>&,
                                   uint32_t queue_family_index)>;
  bool Initialize(
      uint32_t options,
      const VulkanInfo& info,
      const std::vector<const char*>& required_extensions,
      bool allow_protected_memory,
      const GetPresentationSupportCallback& get_presentation_support);

  bool InitializeForWebView(VkPhysicalDevice vk_physical_device,
                            VkDevice vk_device,
                            VkQueue vk_queue,
                            uint32_t vk_queue_index,
                            gfx::ExtensionSet enabled_extensions);

  const gfx::ExtensionSet& enabled_extensions() const {
    return enabled_extensions_;
  }

  void Destroy();

  VkPhysicalDevice GetVulkanPhysicalDevice() const {
    DCHECK_NE(static_cast<VkPhysicalDevice>(VK_NULL_HANDLE),
              vk_physical_device_);
    return vk_physical_device_;
  }

  const VkPhysicalDeviceProperties& vk_physical_device_properties() const {
    return vk_physical_device_properties_;
  }

  VkDevice GetVulkanDevice() const {
    DCHECK_NE(static_cast<VkDevice>(VK_NULL_HANDLE), vk_device_);
    return vk_device_;
  }

  VkQueue GetVulkanQueue() const {
    DCHECK_NE(static_cast<VkQueue>(VK_NULL_HANDLE), vk_queue_);
    return vk_queue_;
  }

  VkInstance GetVulkanInstance() const { return vk_instance_; }

  uint32_t GetVulkanQueueIndex() const { return vk_queue_index_; }

  std::unique_ptr<gpu::VulkanCommandPool> CreateCommandPool();

  VulkanFenceHelper* GetFenceHelper() const { return cleanup_helper_.get(); }

  const VkPhysicalDeviceFeatures2& enabled_device_features_2() const {
    return enabled_device_features_2_;
  }

  const VkPhysicalDeviceFeatures& enabled_device_features() const {
    return enabled_device_features_2_.features;
  }

  bool allow_protected_memory() const { return allow_protected_memory_; }

 private:
  gfx::ExtensionSet enabled_extensions_;
  VkPhysicalDevice vk_physical_device_ = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties vk_physical_device_properties_;
  VkDevice owned_vk_device_ = VK_NULL_HANDLE;
  VkDevice vk_device_ = VK_NULL_HANDLE;
  VkQueue vk_queue_ = VK_NULL_HANDLE;
  uint32_t vk_queue_index_ = 0;
  const VkInstance vk_instance_;
  std::unique_ptr<VulkanFenceHelper> cleanup_helper_;
  VkPhysicalDeviceFeatures2 enabled_device_features_2_;

  const bool enforce_protected_memory_;
  bool allow_protected_memory_ = false;

#if defined(OS_ANDROID) || defined(OS_FUCHSIA)
  VkPhysicalDeviceSamplerYcbcrConversionFeatures
      sampler_ycbcr_conversion_features_;
#endif  // defined(OS_ANDROID) || defined(OS_FUCHSIA)

  VkPhysicalDeviceProtectedMemoryFeatures protected_memory_features_;

  DISALLOW_COPY_AND_ASSIGN(VulkanDeviceQueue);
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_DEVICE_QUEUE_H_

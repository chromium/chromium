// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_device_queue.h"

#include <unordered_set>
#include <utility>
#include <vector>

#include "gpu/config/vulkan_info.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

VulkanDeviceQueue::VulkanDeviceQueue(VkInstance vk_instance,
                                     bool enforce_protected_memory)
    : vk_instance_(vk_instance),
      enforce_protected_memory_(enforce_protected_memory) {}

VulkanDeviceQueue::~VulkanDeviceQueue() {
  DCHECK_EQ(static_cast<VkPhysicalDevice>(VK_NULL_HANDLE), vk_physical_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), vk_device_);
  DCHECK_EQ(static_cast<VkQueue>(VK_NULL_HANDLE), vk_queue_);
}

bool VulkanDeviceQueue::Initialize(
    uint32_t options,
    const VulkanInfo& info,
    const std::vector<const char*>& required_extensions,
    bool allow_protected_memory,
    const GetPresentationSupportCallback& get_presentation_support) {
  DCHECK_EQ(static_cast<VkPhysicalDevice>(VK_NULL_HANDLE), vk_physical_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), owned_vk_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), vk_device_);
  DCHECK_EQ(static_cast<VkQueue>(VK_NULL_HANDLE), vk_queue_);
  DCHECK(!enforce_protected_memory_ || allow_protected_memory);

  if (VK_NULL_HANDLE == vk_instance_)
    return false;

  VkResult result = VK_SUCCESS;

  VkQueueFlags queue_flags = 0;
  if (options & DeviceQueueOption::GRAPHICS_QUEUE_FLAG)
    queue_flags |= VK_QUEUE_GRAPHICS_BIT;

  int device_index = -1;
  int queue_index = -1;
  for (size_t i = 0; i < info.physical_devices.size(); ++i) {
    const auto& device_info = info.physical_devices[i];
    const VkPhysicalDevice& device = device_info.device;
    for (size_t n = 0; n < device_info.queue_families.size(); ++n) {
      if ((device_info.queue_families[n].queueFlags & queue_flags) !=
          queue_flags)
        continue;

      if (options & DeviceQueueOption::PRESENTATION_SUPPORT_QUEUE_FLAG &&
          !get_presentation_support.Run(device, device_info.queue_families,
                                        n)) {
        continue;
      }

      queue_index = static_cast<int>(n);
      break;
    }
    if (-1 != queue_index) {
      device_index = static_cast<int>(i);
      break;
    }
  }

  if (queue_index == -1)
    return false;

  const auto& physical_device_info = info.physical_devices[device_index];
  vk_physical_device_ = physical_device_info.device;
  vk_physical_device_properties_ = physical_device_info.properties;
  vk_queue_index_ = queue_index;

  float queue_priority = 0.0f;
  VkDeviceQueueCreateInfo queue_create_info = {};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = queue_index;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;
  queue_create_info.flags =
      allow_protected_memory ? VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT : 0;

  std::vector<const char*> enabled_layer_names;
#if DCHECK_IS_ON()
  std::unordered_set<std::string> desired_layers({
      "VK_LAYER_KHRONOS_validation",
  });

  for (const auto& layer : physical_device_info.layers) {
    if (desired_layers.find(layer.layerName) != desired_layers.end())
      enabled_layer_names.push_back(layer.layerName);
  }
#endif  // DCHECK_IS_ON()

  std::vector<const char*> enabled_extensions;
  enabled_extensions.insert(std::end(enabled_extensions),
                            std::begin(required_extensions),
                            std::end(required_extensions));

  uint32_t device_api_version = std::min(
      info.used_api_version, vk_physical_device_properties_.apiVersion);

  // Disable all physical device features by default.
  enabled_device_features_2_ = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

  // Android and Fuchsia need YCbCr sampler support.
#if defined(OS_ANDROID) || defined(OS_FUCHSIA)
  if (!physical_device_info.feature_sampler_ycbcr_conversion) {
    LOG(ERROR) << "samplerYcbcrConversion is not supported.";
    return false;
  }
  sampler_ycbcr_conversion_features_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES};
  sampler_ycbcr_conversion_features_.samplerYcbcrConversion = VK_TRUE;

  // Add VkPhysicalDeviceSamplerYcbcrConversionFeatures struct to pNext chain
  // of VkPhysicalDeviceFeatures2 to enable YCbCr sampler support.
  sampler_ycbcr_conversion_features_.pNext = enabled_device_features_2_.pNext;
  enabled_device_features_2_.pNext = &sampler_ycbcr_conversion_features_;
#endif  // defined(OS_ANDROID) || defined(OS_FUCHSIA)

  if (allow_protected_memory) {
    if (!physical_device_info.feature_protected_memory) {
      DLOG(ERROR) << "Protected memory is not supported";
      return false;
    }
    protected_memory_features_ = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES};
    protected_memory_features_.protectedMemory = VK_TRUE;

    // Add VkPhysicalDeviceProtectedMemoryFeatures struct to pNext chain
    // of VkPhysicalDeviceFeatures2 to enable YCbCr sampler support.
    protected_memory_features_.pNext = enabled_device_features_2_.pNext;
    enabled_device_features_2_.pNext = &protected_memory_features_;
  }

  VkDeviceCreateInfo device_create_info = {
      VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  device_create_info.pNext = enabled_device_features_2_.pNext;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.enabledLayerCount = enabled_layer_names.size();
  device_create_info.ppEnabledLayerNames = enabled_layer_names.data();
  device_create_info.enabledExtensionCount = enabled_extensions.size();
  device_create_info.ppEnabledExtensionNames = enabled_extensions.data();
  device_create_info.pEnabledFeatures = &enabled_device_features_2_.features;

  result = vkCreateDevice(vk_physical_device_, &device_create_info, nullptr,
                          &owned_vk_device_);
  if (VK_SUCCESS != result)
    return false;

  enabled_extensions_ = gfx::ExtensionSet(std::begin(enabled_extensions),
                                          std::end(enabled_extensions));

  if (!gpu::GetVulkanFunctionPointers()->BindDeviceFunctionPointers(
          owned_vk_device_, device_api_version, enabled_extensions_)) {
    vkDestroyDevice(owned_vk_device_, nullptr);
    owned_vk_device_ = VK_NULL_HANDLE;
    return false;
  }

  vk_device_ = owned_vk_device_;

  if (allow_protected_memory) {
    VkDeviceQueueInfo2 queue_info2 = {};
    queue_info2.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
    queue_info2.flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
    queue_info2.queueFamilyIndex = queue_index;
    queue_info2.queueIndex = 0;
    vkGetDeviceQueue2(vk_device_, &queue_info2, &vk_queue_);
  } else {
    vkGetDeviceQueue(vk_device_, queue_index, 0, &vk_queue_);
  }

  cleanup_helper_ = std::make_unique<VulkanFenceHelper>(this);

  allow_protected_memory_ = allow_protected_memory;

  return true;
}

bool VulkanDeviceQueue::InitializeForWebView(
    VkPhysicalDevice vk_physical_device,
    VkDevice vk_device,
    VkQueue vk_queue,
    uint32_t vk_queue_index,
    gfx::ExtensionSet enabled_extensions) {
  DCHECK_EQ(static_cast<VkPhysicalDevice>(VK_NULL_HANDLE), vk_physical_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), owned_vk_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), vk_device_);
  DCHECK_EQ(static_cast<VkQueue>(VK_NULL_HANDLE), vk_queue_);

  vk_physical_device_ = vk_physical_device;
  vk_device_ = vk_device;
  vk_queue_ = vk_queue;
  vk_queue_index_ = vk_queue_index;
  enabled_extensions_ = std::move(enabled_extensions);

  cleanup_helper_ = std::make_unique<VulkanFenceHelper>(this);
  return true;
}

void VulkanDeviceQueue::Destroy() {
  if (cleanup_helper_) {
    cleanup_helper_->Destroy();
    cleanup_helper_.reset();
  }

  if (VK_NULL_HANDLE != owned_vk_device_) {
    vkDestroyDevice(owned_vk_device_, nullptr);
    owned_vk_device_ = VK_NULL_HANDLE;
  }
  vk_device_ = VK_NULL_HANDLE;
  vk_queue_ = VK_NULL_HANDLE;
  vk_queue_index_ = 0;
  vk_physical_device_ = VK_NULL_HANDLE;
}

std::unique_ptr<VulkanCommandPool> VulkanDeviceQueue::CreateCommandPool() {
  std::unique_ptr<VulkanCommandPool> command_pool(new VulkanCommandPool(this));
  if (!command_pool->Initialize(enforce_protected_memory_))
    return nullptr;

  return command_pool;
}

}  // namespace gpu

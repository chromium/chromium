// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_device_queue.h"

#include <cstring>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "gpu/config/gpu_info.h"  // nogncheck
#include "gpu/config/vulkan_info.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_crash_keys.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"

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
    const GPUInfo* gpu_info,
    const VulkanInfo& info,
    const std::vector<const char*>& required_extensions,
    const std::vector<const char*>& optional_extensions,
    bool allow_protected_memory,
    const GetPresentationSupportCallback& get_presentation_support,
    uint32_t heap_memory_limit) {
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

  // We prefer to use discrete GPU, integrated GPU is the second, and then
  // others.
  static constexpr int kDeviceTypeScores[] = {
      0,  // VK_PHYSICAL_DEVICE_TYPE_OTHER
      3,  // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
      4,  // VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
      2,  // VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU
      1,  // VK_PHYSICAL_DEVICE_TYPE_CPU
  };
  static_assert(VK_PHYSICAL_DEVICE_TYPE_OTHER == 0, "");
  static_assert(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU == 1, "");
  static_assert(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == 2, "");
  static_assert(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU == 3, "");
  static_assert(VK_PHYSICAL_DEVICE_TYPE_CPU == 4, "");

  int device_index = -1;
  int queue_index = -1;
  int device_score = -1;
  for (size_t i = 0; i < info.physical_devices.size(); ++i) {
    const auto& device_info = info.physical_devices[i];
    const auto& device_properties = device_info.properties;
    if (device_properties.apiVersion < info.used_api_version)
      continue;

    // If gpu_info is provided, the device should match it.
    if (gpu_info && (device_properties.vendorID != gpu_info->gpu.vendor_id ||
                     device_properties.deviceID != gpu_info->gpu.device_id)) {
      continue;
    }

    if (device_properties.deviceType < 0 ||
        device_properties.deviceType > VK_PHYSICAL_DEVICE_TYPE_CPU) {
      DLOG(ERROR) << "Unsupported device type: "
                  << device_properties.deviceType;
      continue;
    }

    const VkPhysicalDevice& device = device_info.device;
    bool found = false;
    for (size_t n = 0; n < device_info.queue_families.size(); ++n) {
      if ((device_info.queue_families[n].queueFlags & queue_flags) !=
          queue_flags) {
        continue;
      }

      if (options & DeviceQueueOption::PRESENTATION_SUPPORT_QUEUE_FLAG &&
          !get_presentation_support.Run(device, device_info.queue_families,
                                        n)) {
        continue;
      }

      if (kDeviceTypeScores[device_properties.deviceType] > device_score) {
        device_index = i;
        queue_index = static_cast<int>(n);
        device_score = kDeviceTypeScores[device_properties.deviceType];
        found = true;
        break;
      }
    }
    
    if (!found)
      continue;

    // Use the device, if it matches gpu_info.
    if (gpu_info)
      break;

    // If the device is a discrete GPU, we will use it. Otherwise go through
    // all the devices and find the device with the highest score.
    if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
      break;
  }

  if (device_index == -1) {
    DLOG(ERROR) << "Cannot find capable device.";
    return false;
  }

  const auto& physical_device_info = info.physical_devices[device_index];
  vk_physical_device_ = physical_device_info.device;
  vk_physical_device_properties_ = physical_device_info.properties;
  vk_physical_device_driver_properties_ =
      physical_device_info.driver_properties;
  vk_queue_index_ = queue_index;

  float queue_priority = 0.0f;
  VkDeviceQueueCreateInfo queue_create_info = {};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = queue_index;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;
  queue_create_info.flags =
      allow_protected_memory ? VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT : 0;

  std::vector<const char*> enabled_extensions;
  for (const char* extension : required_extensions) {
    const auto it =
        std::find_if(physical_device_info.extensions.begin(),
                     physical_device_info.extensions.end(),
                     [extension](const VkExtensionProperties& p) {
                       return std::strcmp(extension, p.extensionName) == 0;
                     });
    if (it == physical_device_info.extensions.end()) {
      // On Fuchsia, some device extensions are provided by layers.
      // TODO(penghuang): checking extensions against layer device extensions
      // too.
#if !defined(OS_FUCHSIA)
      DLOG(ERROR) << "Required Vulkan extension " << extension
                  << " is not supported.";
      return false;
#endif
    }
    enabled_extensions.push_back(extension);
  }

  for (const char* extension : optional_extensions) {
    const auto it =
        std::find_if(physical_device_info.extensions.begin(),
                     physical_device_info.extensions.end(),
                     [extension](const VkExtensionProperties& p) {
                       return std::strcmp(extension, p.extensionName) == 0;
                     });
    if (it == physical_device_info.extensions.end()) {
      DLOG(ERROR) << "Optional Vulkan extension " << extension
                  << " is not supported.";
    } else {
      enabled_extensions.push_back(extension);
    }
  }

  crash_keys::vulkan_device_api_version.Set(
      VkVersionToString(vk_physical_device_properties_.apiVersion));
  crash_keys::vulkan_device_driver_version.Set(base::StringPrintf(
      "0x%08x", vk_physical_device_properties_.driverVersion));
  crash_keys::vulkan_device_vendor_id.Set(
      base::StringPrintf("0x%04x", vk_physical_device_properties_.vendorID));
  crash_keys::vulkan_device_id.Set(
      base::StringPrintf("0x%04x", vk_physical_device_properties_.deviceID));
  static const char* kDeviceTypeNames[] = {
      "other", "integrated", "discrete", "virtual", "cpu",
  };
  uint32_t gpu_type = vk_physical_device_properties_.deviceType;
  if (gpu_type >= base::size(kDeviceTypeNames))
    gpu_type = 0;
  crash_keys::vulkan_device_type.Set(kDeviceTypeNames[gpu_type]);
  crash_keys::vulkan_device_name.Set(vk_physical_device_properties_.deviceName);

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
  device_create_info.enabledExtensionCount = enabled_extensions.size();
  device_create_info.ppEnabledExtensionNames = enabled_extensions.data();
  device_create_info.pEnabledFeatures = &enabled_device_features_2_.features;

  result = vkCreateDevice(vk_physical_device_, &device_create_info, nullptr,
                          &owned_vk_device_);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateDevice failed. result:" << result;
    return false;
  }

  enabled_extensions_ = gfx::ExtensionSet(std::begin(enabled_extensions),
                                          std::end(enabled_extensions));

  if (!gpu::GetVulkanFunctionPointers()->BindDeviceFunctionPointers(
          owned_vk_device_, info.used_api_version, enabled_extensions_)) {
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

  std::vector<VkDeviceSize> heap_size_limit(
      VK_MAX_MEMORY_HEAPS,
      heap_memory_limit ? heap_memory_limit : VK_WHOLE_SIZE);
  vma::CreateAllocator(vk_physical_device_, vk_device_, vk_instance_,
                       heap_size_limit.data(), &vma_allocator_);
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

  vma::CreateAllocator(vk_physical_device_, vk_device_, vk_instance_, nullptr,
                       &vma_allocator_);

  cleanup_helper_ = std::make_unique<VulkanFenceHelper>(this);
  return true;
}

void VulkanDeviceQueue::Destroy() {
  if (cleanup_helper_) {
    cleanup_helper_->Destroy();
    cleanup_helper_.reset();
  }

  if (vma_allocator_ != VK_NULL_HANDLE) {
    vma::DestroyAllocator(vma_allocator_);
    vma_allocator_ = VK_NULL_HANDLE;
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

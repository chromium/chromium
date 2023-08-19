// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_implementation.h"

#include "base/functional/bind.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_util.h"

namespace gpu {

VulkanImplementation::VulkanImplementation(bool use_swiftshader,
                                           bool allow_protected_memory)
    : use_swiftshader_(use_swiftshader),
      allow_protected_memory_(allow_protected_memory) {}

VulkanImplementation::~VulkanImplementation() {}

std::unique_ptr<VulkanDeviceQueue> CreateVulkanDeviceQueue(
    VulkanImplementation* vulkan_implementation,
    uint32_t option,
    const GPUInfo* gpu_info,
    uint32_t heap_memory_limit,
    const bool is_thread_safe) {
  auto* instance = vulkan_implementation->GetVulkanInstance();
  auto device_queue = std::make_unique<VulkanDeviceQueue>(instance);
  auto callback = base::BindRepeating(
      &VulkanImplementation::GetPhysicalDevicePresentationSupport,
      base::Unretained(vulkan_implementation));
  std::vector<const char*> required_extensions =
      vulkan_implementation->GetRequiredDeviceExtensions();
  std::vector<const char*> optional_extensions =
      vulkan_implementation->GetOptionalDeviceExtensions();

  if (instance->is_from_angle()) {
    if (!device_queue->InitializeFromANGLE()) {
      device_queue->Destroy();
      return nullptr;
    }
  } else {
    if (!device_queue->Initialize(
            option, gpu_info, std::move(required_extensions),
            std::move(optional_extensions),
            vulkan_implementation->allow_protected_memory(), callback,
            heap_memory_limit, is_thread_safe)) {
      device_queue->Destroy();
      return nullptr;
    }
  }

  return device_queue;
}

VkSemaphore VulkanImplementation::CreateExternalSemaphore(VkDevice vk_device) {
  return CreateExternalVkSemaphore(vk_device, GetExternalSemaphoreHandleType());
}

VkSemaphore VulkanImplementation::ImportSemaphoreHandle(
    VkDevice vk_device,
    SemaphoreHandle sync_handle) {
  return ImportVkSemaphoreHandle(vk_device, std::move(sync_handle));
}

SemaphoreHandle VulkanImplementation::GetSemaphoreHandle(
    VkDevice vk_device,
    VkSemaphore vk_semaphore) {
  return GetVkSemaphoreHandle(vk_device, vk_semaphore,
                              GetExternalSemaphoreHandleType());
}

bool VulkanImplementation::IsExternalSemaphoreSupported(
    VulkanDeviceQueue* device_queue) {
  return IsVkExternalSemaphoreHandleTypeSupported(
      device_queue, GetExternalSemaphoreHandleType());
}

}  // namespace gpu

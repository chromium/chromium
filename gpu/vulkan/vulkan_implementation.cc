// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_implementation.h"

#include "base/bind.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"

namespace gpu {

VulkanImplementation::VulkanImplementation(bool use_swiftshader,
                                           bool allow_protected_memory,
                                           bool enforce_protected_memory)
    : use_swiftshader_(use_swiftshader),
      allow_protected_memory_(allow_protected_memory),
      enforce_protected_memory_(enforce_protected_memory) {}

VulkanImplementation::~VulkanImplementation() {}

std::unique_ptr<VulkanDeviceQueue> CreateVulkanDeviceQueue(
    VulkanImplementation* vulkan_implementation,
    uint32_t option) {
  auto device_queue = std::make_unique<VulkanDeviceQueue>(
      vulkan_implementation->GetVulkanInstance()->vk_instance(),
      vulkan_implementation->enforce_protected_memory());
  auto callback = base::BindRepeating(
      &VulkanImplementation::GetPhysicalDevicePresentationSupport,
      base::Unretained(vulkan_implementation));
  std::vector<const char*> required_extensions =
      vulkan_implementation->GetRequiredDeviceExtensions();
  if (!device_queue->Initialize(
          option, vulkan_implementation->GetVulkanInstance()->vulkan_info(),
          std::move(required_extensions),
          vulkan_implementation->allow_protected_memory(), callback)) {
    device_queue->Destroy();
    return nullptr;
  }

  return device_queue;
}

}  // namespace gpu

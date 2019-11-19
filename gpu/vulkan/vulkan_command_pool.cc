// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_command_pool.h"

#include "base/logging.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

VulkanCommandPool::VulkanCommandPool(VulkanDeviceQueue* device_queue)
    : device_queue_(device_queue) {}

VulkanCommandPool::~VulkanCommandPool() {
  DCHECK_EQ(0u, command_buffer_count_);
  DCHECK_EQ(static_cast<VkCommandPool>(VK_NULL_HANDLE), handle_);
}

bool VulkanCommandPool::Initialize(bool use_protected_memory) {
  VkCommandPoolCreateInfo command_pool_create_info = {};
  command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_create_info.flags =
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  command_pool_create_info.queueFamilyIndex =
      device_queue_->GetVulkanQueueIndex();
  if (use_protected_memory) {
    command_pool_create_info.flags |= VK_COMMAND_POOL_CREATE_PROTECTED_BIT;
  }

  VkResult result =
      vkCreateCommandPool(device_queue_->GetVulkanDevice(),
                          &command_pool_create_info, nullptr, &handle_);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateCommandPool() failed: " << result;
    return false;
  }

  use_protected_memory_ = use_protected_memory;

  return true;
}

void VulkanCommandPool::Destroy() {
  DCHECK_EQ(0u, command_buffer_count_);
  if (VK_NULL_HANDLE != handle_) {
    vkDestroyCommandPool(device_queue_->GetVulkanDevice(), handle_, nullptr);
    handle_ = VK_NULL_HANDLE;
  }
}

std::unique_ptr<VulkanCommandBuffer>
VulkanCommandPool::CreatePrimaryCommandBuffer() {
  std::unique_ptr<VulkanCommandBuffer> command_buffer(new VulkanCommandBuffer(
      device_queue_, this, true, use_protected_memory_));
  if (!command_buffer->Initialize())
    return nullptr;

  return command_buffer;
}

std::unique_ptr<VulkanCommandBuffer>
VulkanCommandPool::CreateSecondaryCommandBuffer() {
  auto command_buffer = std::make_unique<VulkanCommandBuffer>(
      device_queue_, this, false, use_protected_memory_);
  if (!command_buffer->Initialize())
    return nullptr;

  return command_buffer;
}

void VulkanCommandPool::IncrementCommandBufferCount() {
  command_buffer_count_++;
}

void VulkanCommandPool::DecrementCommandBufferCount() {
  DCHECK_LT(0u, command_buffer_count_);
  command_buffer_count_--;
}

}  // namespace gpu

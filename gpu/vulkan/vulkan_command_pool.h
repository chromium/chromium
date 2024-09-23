// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_COMMAND_POOL_H_
#define GPU_VULKAN_VULKAN_COMMAND_POOL_H_

#include <vulkan/vulkan_core.h>

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

namespace gpu {

class VulkanCommandBuffer;
class VulkanDeviceQueue;

class COMPONENT_EXPORT(VULKAN) VulkanCommandPool {
 public:
  explicit VulkanCommandPool(VulkanDeviceQueue* device_queue);

  VulkanCommandPool(const VulkanCommandPool&) = delete;
  VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;

  ~VulkanCommandPool();

  bool Initialize(bool allow_protected_memory = false);
  // Destroy() should be called when all related GPU tasks have been finished.
  void Destroy();

  std::unique_ptr<VulkanCommandBuffer> CreatePrimaryCommandBuffer();
  std::unique_ptr<VulkanCommandBuffer> CreateSecondaryCommandBuffer();

  VkCommandPool handle() { return handle_; }

 private:
  friend class VulkanCommandBuffer;

  void IncrementCommandBufferCount();
  void DecrementCommandBufferCount();

  raw_ptr<VulkanDeviceQueue> device_queue_;
  VkCommandPool handle_ = VK_NULL_HANDLE;
  uint32_t command_buffer_count_ = 0;
  bool use_protected_memory_ = false;
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_COMMAND_POOL_H_

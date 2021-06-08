// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_image.h"

#include "base/logging.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

bool VulkanImage::InitializeFromGpuMemoryBufferHandle(
    VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    const gfx::Size& size,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkImageTiling image_tiling) {
  NOTIMPLEMENTED();
  return false;
}

base::win::ScopedHandle VulkanImage::GetMemoryHandle(
    VkExternalMemoryHandleTypeFlagBits handle_type) {
  VkMemoryGetWin32HandleInfoKHR get_handle_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
      .memory = device_memory_,
      .handleType = handle_type,
  };

  VkDevice device = device_queue_->GetVulkanDevice();

  HANDLE handle = nullptr;
  vkGetMemoryWin32HandleKHR(device, &get_handle_info, &handle);
  if (handle == nullptr) {
    DLOG(ERROR) << "Unable to extract file handle out of external VkImage";
    return base::win::ScopedHandle();
  }

  return base::win::ScopedHandle(handle);
}

}  // namespace gpu

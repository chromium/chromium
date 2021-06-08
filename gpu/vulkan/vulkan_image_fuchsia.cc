// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_image.h"

#include "base/logging.h"
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
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

zx::vmo VulkanImage::GetMemoryZirconHandle() {
  DCHECK(handle_types_ &
         VK_EXTERNAL_MEMORY_HANDLE_TYPE_TEMP_ZIRCON_VMO_BIT_FUCHSIA);
  VkMemoryGetZirconHandleInfoFUCHSIA get_handle_info = {
      .sType = VK_STRUCTURE_TYPE_TEMP_MEMORY_GET_ZIRCON_HANDLE_INFO_FUCHSIA,
      .memory = device_memory_,
      .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_TEMP_ZIRCON_VMO_BIT_FUCHSIA,
  };

  VkDevice device = device_queue_->GetVulkanDevice();
  zx::vmo vmo;
  VkResult result = vkGetMemoryZirconHandleFUCHSIA(device, &get_handle_info,
                                                   vmo.reset_and_get_address());
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkGetMemoryFuchsiaHandleKHR failed: " << result;
    vmo.reset();
  }

  return vmo;
}

}  // namespace gpu

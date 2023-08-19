// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_util.h"

#include "base/logging.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

VkSemaphore ImportVkSemaphoreHandle(VkDevice vk_device,
                                    SemaphoreHandle handle) {
  auto handle_type = handle.vk_handle_type();
  if (!handle.is_valid() ||
      (handle_type != VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT &&
       handle_type != VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)) {
    return VK_NULL_HANDLE;
  }

  VkSemaphore semaphore = VK_NULL_HANDLE;
  VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkResult result = vkCreateSemaphore(vk_device, &info, nullptr, &semaphore);
  if (result != VK_SUCCESS)
    return VK_NULL_HANDLE;
  base::ScopedFD fd = handle.TakeHandle();
  const auto is_sync_fd =
      handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
  const VkImportSemaphoreFdInfoKHR import = {
      .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR,
      .semaphore = semaphore,
      .flags = is_sync_fd ? VK_SEMAPHORE_IMPORT_TEMPORARY_BIT_KHR
                          : VkSemaphoreImportFlags{0},
      .handleType = handle_type,
      .fd = fd.release(),
  };

  result = vkImportSemaphoreFdKHR(vk_device, &import);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkImportSemaphoreFdKHR failed: " << result;
    vkDestroySemaphore(vk_device, semaphore, nullptr);
    // If import failed, we need to close fd manually.
    base::ScopedFD close_fd(import.fd);
    return VK_NULL_HANDLE;
  }

  return semaphore;
}

SemaphoreHandle GetVkSemaphoreHandle(
    VkDevice vk_device,
    VkSemaphore vk_semaphore,
    VkExternalSemaphoreHandleTypeFlagBits handle_type) {
  VkSemaphoreGetFdInfoKHR info = {VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
  info.semaphore = vk_semaphore;
  info.handleType = handle_type;

  int fd = -1;
  VkResult result = vkGetSemaphoreFdKHR(vk_device, &info, &fd);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkGetSemaphoreFdKHR failed: " << result;
    return SemaphoreHandle();
  }

  return SemaphoreHandle(handle_type, base::ScopedFD(fd));
}

bool IsVkOpaqueExternalSemaphoreSupported(VulkanDeviceQueue* device_queue) {
  return IsVkExternalSemaphoreHandleTypeSupported(
      device_queue, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);
}

VkSemaphore CreateVkOpaqueExternalSemaphore(VkDevice vk_device) {
  return CreateExternalVkSemaphore(
      vk_device, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);
}

SemaphoreHandle ExportVkOpaqueExternalSemaphore(VkDevice vk_device,
                                                VkSemaphore vk_semaphore) {
  return GetVkSemaphoreHandle(vk_device, vk_semaphore,
                              VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);
}

}  // namespace gpu

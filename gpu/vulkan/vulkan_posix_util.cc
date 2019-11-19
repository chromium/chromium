// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_posix_util.h"

#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

VkSemaphore ImportVkSemaphoreHandlePosix(VkDevice vk_device,
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
  VkImportSemaphoreFdInfoKHR import = {
      VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR};
  import.semaphore = semaphore;
  if (handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT)
    import.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT_KHR;
  import.handleType = handle_type;
  import.fd = fd.get();

  result = vkImportSemaphoreFdKHR(vk_device, &import);
  if (result != VK_SUCCESS) {
    vkDestroySemaphore(vk_device, semaphore, nullptr);
    return VK_NULL_HANDLE;
  }

  // If import is successful, the VkSemaphore takes the ownership of the fd.
  ignore_result(fd.release());

  return semaphore;
}

SemaphoreHandle GetVkSemaphoreHandlePosix(
    VkDevice vk_device,
    VkSemaphore vk_semaphore,
    VkExternalSemaphoreHandleTypeFlagBits handle_type) {
  VkSemaphoreGetFdInfoKHR info = {VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
  info.semaphore = vk_semaphore;
  info.handleType = handle_type;

  int fd = -1;
  VkResult result = vkGetSemaphoreFdKHR(vk_device, &info, &fd);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkGetSemaphoreFdKHR failed : " << result;
    return SemaphoreHandle();
  }

  return SemaphoreHandle(handle_type, base::ScopedFD(fd));
}

}  // namespace gpu

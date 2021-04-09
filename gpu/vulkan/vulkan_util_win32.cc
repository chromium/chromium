// Copyright 2019 The Chromium Authors. All rights reserved.
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

      handle_type != VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT) {
    return VK_NULL_HANDLE;
  }

  VkSemaphore semaphore = VK_NULL_HANDLE;
  VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkResult result = vkCreateSemaphore(vk_device, &info, nullptr, &semaphore);
  if (result != VK_SUCCESS)
    return VK_NULL_HANDLE;

  auto win32_handle = handle.TakeHandle();
  VkImportSemaphoreWin32HandleInfoKHR import = {
      .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
      .semaphore = semaphore,
      .handleType = handle_type,
      .handle = win32_handle.Get(),
  };
  result = vkImportSemaphoreWin32HandleKHR(vk_device, &import);
  if (result != VK_SUCCESS) {
    vkDestroySemaphore(vk_device, semaphore, nullptr);
    return VK_NULL_HANDLE;
  }

  // If import is successful, the VkSemaphore takes the ownership of the fd.
  ignore_result(win32_handle.Take());

  return semaphore;
}

SemaphoreHandle GetVkSemaphoreHandle(
    VkDevice vk_device,
    VkSemaphore vk_semaphore,
    VkExternalSemaphoreHandleTypeFlagBits handle_type) {
  VkSemaphoreGetWin32HandleInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
      .semaphore = vk_semaphore,
      .handleType = handle_type,
  };

  HANDLE handle = nullptr;
  VkResult result = vkGetSemaphoreWin32HandleKHR(vk_device, &info, &handle);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkGetSemaphoreFdKHR failed : " << result;
    return SemaphoreHandle();
  }

  return SemaphoreHandle(handle_type, base::win::ScopedHandle(handle));
}

}  // namespace gpu

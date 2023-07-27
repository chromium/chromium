// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_util.h"

#include "base/logging.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

VkSemaphore ImportVkSemaphoreHandle(VkDevice vk_device,
                                    SemaphoreHandle handle) {
  if (!handle.is_valid() ||
      handle.vk_handle_type() !=
          VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA) {
    return VK_NULL_HANDLE;
  }

  VkSemaphore semaphore = VK_NULL_HANDLE;
  VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkResult result = vkCreateSemaphore(vk_device, &info, nullptr, &semaphore);
  if (result != VK_SUCCESS)
    return VK_NULL_HANDLE;

  zx::event event = handle.TakeHandle();
  VkImportSemaphoreZirconHandleInfoFUCHSIA import = {
      VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_ZIRCON_HANDLE_INFO_FUCHSIA};
  import.semaphore = semaphore;
  import.handleType =
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA;
  import.zirconHandle = event.get();

  result = vkImportSemaphoreZirconHandleFUCHSIA(vk_device, &import);
  if (result != VK_SUCCESS) {
    vkDestroySemaphore(vk_device, semaphore, nullptr);
    return VK_NULL_HANDLE;
  }

  // Vulkan took ownership of the handle.
  std::ignore = event.release();

  return semaphore;
}

SemaphoreHandle GetVkSemaphoreHandle(
    VkDevice vk_device,
    VkSemaphore vk_semaphore,
    VkExternalSemaphoreHandleTypeFlagBits handle_type) {
  DCHECK_EQ(handle_type,
            VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA);

  // Create VkSemaphoreGetFdInfoKHR structure.
  VkSemaphoreGetZirconHandleInfoFUCHSIA info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_GET_ZIRCON_HANDLE_INFO_FUCHSIA};
  info.semaphore = vk_semaphore;
  info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA;

  zx_handle_t handle;
  VkResult result =
      vkGetSemaphoreZirconHandleFUCHSIA(vk_device, &info, &handle);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkGetSemaphoreFuchsiaHandleKHR failed : " << result;
    return gpu::SemaphoreHandle();
  }

  return gpu::SemaphoreHandle(
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA,
      zx::event(handle));
}

bool IsVkOpaqueExternalSemaphoreSupported(VulkanDeviceQueue* device_queue) {
  return IsVkExternalSemaphoreHandleTypeSupported(
      device_queue, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA);
}

VkSemaphore CreateVkOpaqueExternalSemaphore(VkDevice vk_device) {
  return CreateExternalVkSemaphore(
      vk_device, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA);
}

SemaphoreHandle ExportVkOpaqueExternalSemaphore(VkDevice vk_device,
                                                VkSemaphore vk_semaphore) {
  return GetVkSemaphoreHandle(
      vk_device, vk_semaphore,
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA);
}

}  // namespace gpu
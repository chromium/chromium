// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/vulkan/vulkan_memory.h"

#include <vulkan/vulkan.h>

#include <optional>

#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {
namespace {

std::optional<uint32_t> FindMemoryTypeIndex(
    VkPhysicalDevice physical_device,
    const VkMemoryRequirements* requirements,
    VkMemoryPropertyFlags flags) {
  VkPhysicalDeviceMemoryProperties properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);
  constexpr uint32_t kMaxIndex = 31;
  for (uint32_t i = 0; i <= kMaxIndex; i++) {
    if (((1u << i) & requirements->memoryTypeBits) == 0) {
      continue;
    }
    if ((properties.memoryTypes[i].propertyFlags & flags) != flags) {
      continue;
    }
    return i;
  }
  return std::nullopt;
}

}  // namespace

VulkanMemory::VulkanMemory(base::PassKey<VulkanMemory> pass_key) {}

VulkanMemory::~VulkanMemory() {
  DCHECK(!device_queue_);
  DCHECK(device_memory_ == VK_NULL_HANDLE);
}

// static
std::unique_ptr<VulkanMemory> VulkanMemory::Create(
    VulkanDeviceQueue* device_queue,
    VkDeviceMemory device_memory,
    VkDeviceSize size,
    uint32_t type_index) {
  auto memory = std::make_unique<VulkanMemory>(base::PassKey<VulkanMemory>());
  memory->device_queue_ = device_queue;
  memory->device_memory_ = device_memory;
  memory->size_ = size;
  memory->type_index_ = type_index;
  return memory;
}

// static
std::unique_ptr<VulkanMemory> VulkanMemory::Create(
    VulkanDeviceQueue* device_queue,
    const VkMemoryRequirements* requirements,
    const void* extra_allocate_info) {
  auto memory = std::make_unique<VulkanMemory>(base::PassKey<VulkanMemory>());
  if (!memory->Initialize(device_queue, requirements, extra_allocate_info)) {
    return nullptr;
  }
  return memory;
}

void VulkanMemory::Destroy() {
  if (!device_queue_) {
    return;
  }
  VkDevice vk_device = device_queue_->GetVulkanDevice();
  if (device_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(vk_device, device_memory_, nullptr /* pAllocator */);
    device_memory_ = VK_NULL_HANDLE;
  }
  device_queue_ = nullptr;
}

bool VulkanMemory::Initialize(VulkanDeviceQueue* device_queue,
                              const VkMemoryRequirements* requirements,
                              const void* extra_allocate_info) {
  auto index =
      FindMemoryTypeIndex(device_queue->GetVulkanPhysicalDevice(), requirements,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (!index) {
    // Fallback to use any driver advertised memory type when the preferred
    // DEVICE_LOCAL_BIT is not available.
    index = FindMemoryTypeIndex(device_queue->GetVulkanPhysicalDevice(),
                                requirements, 0 /* flags */);
  }
  if (!index) {
    DLOG(ERROR) << "Cannot find validate memory type index.";
    return false;
  }

  VkMemoryAllocateInfo memory_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = extra_allocate_info,
      .allocationSize = requirements->size,
      .memoryTypeIndex = index.value(),
  };

  VkDevice vk_device = device_queue->GetVulkanDevice();
  VkResult result = vkAllocateMemory(vk_device, &memory_allocate_info,
                                     nullptr /* pAllocator */, &device_memory_);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkAllocateMemory failed result:" << result;
    return false;
  }

  device_queue_ = device_queue;
  size_ = requirements->size;
  type_index_ = index.value();

  return true;
}

#if BUILDFLAG(IS_POSIX)
base::ScopedFD VulkanMemory::GetMemoryFd(
    VkExternalMemoryHandleTypeFlagBits handle_type) {
  VkMemoryGetFdInfoKHR get_fd_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
      .memory = device_memory_,
      .handleType = handle_type,

  };

  VkDevice device = device_queue_->GetVulkanDevice();
  int memory_fd = -1;
  vkGetMemoryFdKHR(device, &get_fd_info, &memory_fd);
  if (memory_fd < 0) {
    DLOG(ERROR) << "Unable to extract file descriptor out of external VkImage";
    return base::ScopedFD();
  }

  return base::ScopedFD(memory_fd);
}
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
base::win::ScopedHandle VulkanMemory::GetMemoryHandle(
    VkExternalMemoryHandleTypeFlagBits handle_type) {
  VkMemoryGetWin32HandleInfoKHR get_handle_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
      .memory = device_memory(),
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
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
zx::vmo VulkanMemory::GetMemoryZirconHandle() {
  VkMemoryGetZirconHandleInfoFUCHSIA get_handle_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_GET_ZIRCON_HANDLE_INFO_FUCHSIA,
      .memory = device_memory(),
      .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ZIRCON_VMO_BIT_FUCHSIA,
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
#endif  // BUILDFLAG(IS_FUCHSIA)

}  // namespace gpu

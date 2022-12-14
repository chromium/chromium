// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_MEMORY_H_
#define GPU_VULKAN_VULKAN_MEMORY_H_

#include <vulkan/vulkan_core.h>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/vmo.h>
#endif

namespace gpu {

class VulkanDeviceQueue;

class COMPONENT_EXPORT(VULKAN) VulkanMemory {
 public:
  explicit VulkanMemory(base::PassKey<VulkanMemory> pass_key);
  ~VulkanMemory();

  VulkanMemory(VulkanMemory&) = delete;
  VulkanMemory& operator=(VulkanMemory&) = delete;

  static std::unique_ptr<VulkanMemory> Create(VulkanDeviceQueue* device_queue,
                                              VkDeviceMemory device_memory,
                                              VkDeviceSize size,
                                              uint32_t type_index);

  static std::unique_ptr<VulkanMemory> Create(
      VulkanDeviceQueue* device_queue,
      const VkMemoryRequirements* requirements,
      const void* extra_allocate_info);

  void Destroy();

#if BUILDFLAG(IS_POSIX)
  base::ScopedFD GetMemoryFd(VkExternalMemoryHandleTypeFlagBits handle_type);
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
  base::win::ScopedHandle GetMemoryHandle(
      VkExternalMemoryHandleTypeFlagBits handle_type);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
  zx::vmo GetMemoryZirconHandle();
#endif  // BUILDFLAG(IS_FUCHSIA)

  VulkanDeviceQueue* device_queue() const { return device_queue_; }
  VkDeviceSize size() const { return size_; }
  uint32_t type_index() const { return type_index_; }
  VkDeviceMemory device_memory() const { return device_memory_; }

 private:
  bool Initialize(VulkanDeviceQueue* device_queue,
                  const VkMemoryRequirements* requirements,
                  const void* extra_allocate_info);

  raw_ptr<VulkanDeviceQueue> device_queue_ = nullptr;
  VkDeviceMemory device_memory_ = VK_NULL_HANDLE;
  VkDeviceSize size_ = 0;
  uint32_t type_index_ = 0;
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_MEMORY_H_

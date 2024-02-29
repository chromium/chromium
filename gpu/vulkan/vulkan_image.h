// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_IMAGE_H_
#define GPU_VULKAN_VULKAN_IMAGE_H_

#include <vulkan/vulkan_core.h>

#include <array>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/vulkan/vulkan_memory.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/vmo.h>
#endif

namespace gpu {

class VulkanDeviceQueue;

class COMPONENT_EXPORT(VULKAN) VulkanImage {
 public:
  explicit VulkanImage(base::PassKey<VulkanImage> pass_key);
  ~VulkanImage();

  VulkanImage(VulkanImage&) = delete;
  VulkanImage& operator=(VulkanImage&) = delete;

  static std::unique_ptr<VulkanImage> Create(
      VulkanDeviceQueue* device_queue,
      const gfx::Size& size,
      VkFormat format,
      VkImageUsageFlags usage,
      VkImageCreateFlags flags = 0,
      VkImageTiling image_tiling = VK_IMAGE_TILING_OPTIMAL,
      const void* extra_image_create_info = nullptr,
      const void* extra_memory_allocation_info = nullptr);

  // Create VulkanImage with external memory, it can be exported and used by
  // foreign API
  static std::unique_ptr<VulkanImage> CreateWithExternalMemory(
      VulkanDeviceQueue* device_queue,
      const gfx::Size& size,
      VkFormat format,
      VkImageUsageFlags usage,
      VkImageCreateFlags flags = 0,
      VkImageTiling image_tiling = VK_IMAGE_TILING_OPTIMAL,
      const void* extra_image_create_info = nullptr,
      const void* extra_memory_allocation_info = nullptr);

  static std::unique_ptr<VulkanImage> CreateFromGpuMemoryBufferHandle(
      VulkanDeviceQueue* device_queue,
      gfx::GpuMemoryBufferHandle gmb_handle,
      const gfx::Size& size,
      VkFormat format,
      VkImageUsageFlags usage,
      VkImageCreateFlags flags,
      VkImageTiling image_tiling,
      uint32_t queue_family_index);

  static std::unique_ptr<VulkanImage> Create(
      VulkanDeviceQueue* device_queue,
      VkImage image,
      VkDeviceMemory device_memory,
      const gfx::Size& size,
      VkFormat format,
      VkImageTiling image_tiling,
      VkDeviceSize device_size,
      uint32_t memory_type_index,
      std::optional<VulkanYCbCrInfo>& ycbcr_info,
      VkImageUsageFlags usage,
      VkImageCreateFlags flags);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  static std::unique_ptr<VulkanImage> CreateWithExternalMemoryAndModifiers(
      VulkanDeviceQueue* device_queue,
      const gfx::Size& size,
      VkFormat format,
      std::vector<uint64_t> modifiers,
      VkImageUsageFlags usage,
      VkImageCreateFlags flags);
#endif

  void Destroy();

#if BUILDFLAG(IS_POSIX)
  base::ScopedFD GetMemoryFd(VkExternalMemoryHandleTypeFlagBits handle_type =
                                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT) {
    return memories_[0]->GetMemoryFd(handle_type);
  }
#endif

#if BUILDFLAG(IS_WIN)
  base::win::ScopedHandle GetMemoryHandle(
      VkExternalMemoryHandleTypeFlagBits handle_type =
          VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT) {
    return memories_[0]->GetMemoryHandle(handle_type);
  }
#endif

#if BUILDFLAG(IS_FUCHSIA)
  zx::vmo GetMemoryZirconHandle() {
    return memories_[0]->GetMemoryZirconHandle();
  }
#endif

  VulkanDeviceQueue* device_queue() const { return device_queue_; }
  const VkImageCreateInfo& create_info() const { return create_info_; }
  gfx::Size size() const {
    return gfx::Size(create_info_.extent.width, create_info_.extent.height);
  }
  VkFormat format() const { return create_info_.format; }
  VkImageCreateFlags flags() const { return create_info_.flags; }
  VkImageUsageFlags usage() const { return create_info_.usage; }
  VkDeviceSize device_size(size_t plane = 0) const {
    return memories_[0]->size();
  }
  uint32_t memory_type_index(size_t plane = 0) const {
    return memories_[0]->type_index();
  }
  VkImageTiling image_tiling() const { return create_info_.tiling; }
  uint32_t queue_family_index() const { return queue_family_index_; }
  void set_queue_family_index(uint32_t index) { queue_family_index_ = index; }
  const std::optional<VulkanYCbCrInfo>& ycbcr_info() const {
    return ycbcr_info_;
  }
  VkImage image() const { return image_; }
  VkDeviceMemory device_memory(size_t i = 0) const {
    return memories_[0]->device_memory();
  }
  VkExternalMemoryHandleTypeFlags handle_types() const { return handle_types_; }
  void set_native_pixmap(scoped_refptr<gfx::NativePixmap> pixmap) {
    native_pixmap_ = std::move(pixmap);
  }
  const scoped_refptr<gfx::NativePixmap>& native_pixmap() const {
    return native_pixmap_;
  }
  uint64_t modifier() const { return modifier_; }
  size_t plane_count() const { return plane_count_; }
  const std::array<VkSubresourceLayout, 4>& layouts() const { return layouts_; }

 private:
  // Create VkImage.
  bool CreateVkImage(const gfx::Size& size,
                     VkFormat format,
                     VkImageUsageFlags usage,
                     VkImageCreateFlags flags,
                     VkImageTiling image_tiling,
                     const void* extra_image_create_info);
  // Get memory requirements for the given plane at index.
  VkMemoryRequirements GetMemoryRequirements(size_t plane);
  // Bind memory with the given plane of the image.
  bool BindMemory(size_t plane, std::unique_ptr<VulkanMemory> memory);
  // Allocate memory and bind to the given plane of the image.
  bool AllocateAndBindMemory(size_t plane,
                             const VkMemoryRequirements* requirements,
                             const void* extra_memory_allocation_info);
  // Initialize for single plane or joint planes VkImage
  bool InitializeSingleOrJointPlanes(VulkanDeviceQueue* device_queue,
                                     const gfx::Size& size,
                                     VkFormat format,
                                     VkImageUsageFlags usage,
                                     VkImageCreateFlags flags,
                                     VkImageTiling image_tiling,
                                     const void* extra_image_create_info,
                                     const void* extra_memory_allocation_info,
                                     const VkMemoryRequirements* requirements);
  bool InitializeWithExternalMemory(VulkanDeviceQueue* device_queue,
                                    const gfx::Size& size,
                                    VkFormat format,
                                    VkImageUsageFlags usage,
                                    VkImageCreateFlags flags,
                                    VkImageTiling image_tiling,
                                    const void* extra_image_create_info,
                                    const void* extra_memory_allocation_info);
  bool InitializeFromGpuMemoryBufferHandle(
      VulkanDeviceQueue* device_queue,
      gfx::GpuMemoryBufferHandle gmb_handle,
      const gfx::Size& size,
      VkFormat format,
      VkImageUsageFlags usage,
      VkImageCreateFlags flags,
      VkImageTiling image_tiling,
      uint32_t queue_family_index);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  bool InitializeWithExternalMemoryAndModifiers(VulkanDeviceQueue* device_queue,
                                                const gfx::Size& size,
                                                VkFormat format,
                                                std::vector<uint64_t> modifiers,
                                                VkImageUsageFlags usage,
                                                VkImageCreateFlags flags);
#endif

  raw_ptr<VulkanDeviceQueue> device_queue_ = nullptr;
  VkImageCreateInfo create_info_{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  // Image has multi planes and planes are not joint.
  bool disjoint_planes_ = false;
  uint32_t queue_family_index_ = VK_QUEUE_FAMILY_IGNORED;
  std::optional<VulkanYCbCrInfo> ycbcr_info_;
  VkImage image_ = VK_NULL_HANDLE;
  // Device memory for each plane.
  std::array<std::unique_ptr<VulkanMemory>, 4> memories_;
  VkExternalMemoryHandleTypeFlags handle_types_ = 0;
  scoped_refptr<gfx::NativePixmap> native_pixmap_;
  uint64_t modifier_ = gfx::NativePixmapHandle::kNoModifier;
  size_t plane_count_ = 1;
  std::array<VkSubresourceLayout, 4> layouts_ = {};
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_IMAGE_H_

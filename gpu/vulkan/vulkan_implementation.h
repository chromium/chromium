// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_IMPLEMENTATION_H_
#define GPU_VULKAN_VULKAN_IMPLEMENTATION_H_

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "gpu/vulkan/semaphore_handle.h"
#include "gpu/vulkan/vulkan_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_hardware_buffer_handle.h"
#include "ui/gfx/geometry/size.h"
#endif

namespace gfx {
class GpuFence;
struct GpuMemoryBufferHandle;
}  // namespace gfx

namespace gpu {
class VulkanDeviceQueue;
class VulkanSurface;
class VulkanInstance;
struct VulkanYCbCrInfo;

#if defined(OS_FUCHSIA)
class SysmemBufferCollection {
 public:
  virtual ~SysmemBufferCollection() {}
};
#endif  // defined(OS_FUCHSIA)

// Base class which provides functions for creating vulkan objects for different
// platforms that use platform-specific extensions (e.g. for creation of
// VkSurfaceKHR objects). It also provides helper/utility functions.
class VULKAN_EXPORT VulkanImplementation {
 public:
  VulkanImplementation(bool use_swiftshader = false,
                       bool allow_protected_memory = false,
                       bool enforce_protected_memory = false);

  virtual ~VulkanImplementation();

  // Initialize VulkanInstance. If using_surface, VK_KHR_surface instance
  // extension will be required when initialize VkInstance. This extension is
  // required for presenting with VkSwapchain API.
  virtual bool InitializeVulkanInstance(bool using_surface = true) = 0;

  virtual VulkanInstance* GetVulkanInstance() = 0;

  virtual std::unique_ptr<VulkanSurface> CreateViewSurface(
      gfx::AcceleratedWidget window) = 0;

  virtual bool GetPhysicalDevicePresentationSupport(
      VkPhysicalDevice device,
      const std::vector<VkQueueFamilyProperties>& queue_family_properties,
      uint32_t queue_family_index) = 0;

  virtual std::vector<const char*> GetRequiredDeviceExtensions() = 0;

  // Creates a VkFence that is exportable to a gfx::GpuFence.
  virtual VkFence CreateVkFenceForGpuFence(VkDevice vk_device) = 0;

  // Exports a VkFence to a gfx::GpuFence.
  //
  // The fence should have been created via CreateVkFenceForGpuFence().
  virtual std::unique_ptr<gfx::GpuFence> ExportVkFenceToGpuFence(
      VkDevice vk_device,
      VkFence vk_fence) = 0;

  // Creates a semaphore that can be exported using GetSemaphoreHandle().
  virtual VkSemaphore CreateExternalSemaphore(VkDevice vk_device) = 0;

  // Import a VkSemaphore from a platform-specific handle.
  // Handle types that don't allow permanent import are imported with
  // temporary permanence (VK_SEMAPHORE_IMPORT_TEMPORARY_BIT).
  virtual VkSemaphore ImportSemaphoreHandle(VkDevice vk_device,
                                            SemaphoreHandle handle) = 0;

  // Export a platform-specific handle for a Vulkan semaphore. Returns a null
  // handle in case of a failure.
  virtual SemaphoreHandle GetSemaphoreHandle(VkDevice vk_device,
                                             VkSemaphore vk_semaphore) = 0;

  // Returns VkExternalMemoryHandleTypeFlagBits that should be set when creating
  // external images and memory.
  virtual VkExternalMemoryHandleTypeFlagBits GetExternalImageHandleType() = 0;

  // Returns true if the GpuMemoryBuffer of the specified type can be imported
  // into VkImage using CreateImageFromGpuMemoryHandle().
  virtual bool CanImportGpuMemoryBuffer(
      gfx::GpuMemoryBufferType memory_buffer_type) = 0;

  // Creates a VkImage from a GpuMemoryBuffer. If successful it initializes
  // |vk_image|, |vk_image_info|, |vk_device_memory| and |mem_allocation_size|.
  // Implementation must verify that the specified |size| fits in the size
  // specified when |gmb_handle| was allocated.
  virtual bool CreateImageFromGpuMemoryHandle(
      VkDevice vk_device,
      gfx::GpuMemoryBufferHandle gmb_handle,
      gfx::Size size,
      VkImage* vk_image,
      VkImageCreateInfo* vk_image_info,
      VkDeviceMemory* vk_device_memory,
      VkDeviceSize* mem_allocation_size,
      base::Optional<VulkanYCbCrInfo>* ycbcr_info) = 0;

#if defined(OS_ANDROID)
  // Create a VkImage, import Android AHardwareBuffer object created outside of
  // the Vulkan device into Vulkan memory object and bind it to the VkImage.
  // TODO(sergeyu): Remove this method and use
  // CreateVkImageFromGpuMemoryHandle() instead.
  virtual bool CreateVkImageAndImportAHB(
      const VkDevice& vk_device,
      const VkPhysicalDevice& vk_physical_device,
      const gfx::Size& size,
      base::android::ScopedHardwareBufferHandle ahb_handle,
      VkImage* vk_image,
      VkImageCreateInfo* vk_image_info,
      VkDeviceMemory* vk_device_memory,
      VkDeviceSize* mem_allocation_size,
      VulkanYCbCrInfo* ycbcr_info = nullptr) = 0;

  // Get the sampler ycbcr conversion information from the AHB.
  virtual bool GetSamplerYcbcrConversionInfo(
      const VkDevice& vk_device,
      base::android::ScopedHardwareBufferHandle ahb_handle,
      VulkanYCbCrInfo* ycbcr_info) = 0;
#endif

#if defined(OS_FUCHSIA)
  // Registers as sysmem buffer collection. The collection can be released by
  // destroying the returned SysmemBufferCollection object. Once a collection is
  // registered the individual buffers in the collection can be referenced by
  // using the |id| as |buffer_collection_id| in |gmb_handle| passed to
  // CreateImageFromGpuMemoryHandle().
  virtual std::unique_ptr<SysmemBufferCollection>
  RegisterSysmemBufferCollection(VkDevice device,
                                 gfx::SysmemBufferCollectionId id,
                                 zx::channel token) = 0;
#endif  // defined(OS_FUCHSIA)

  bool use_swiftshader() const { return use_swiftshader_; }
  bool allow_protected_memory() const { return allow_protected_memory_; }
  bool enforce_protected_memory() const { return enforce_protected_memory_; }

 private:
  const bool use_swiftshader_;
  const bool allow_protected_memory_;
  const bool enforce_protected_memory_;
  DISALLOW_COPY_AND_ASSIGN(VulkanImplementation);
};

VULKAN_EXPORT
std::unique_ptr<VulkanDeviceQueue> CreateVulkanDeviceQueue(
    VulkanImplementation* vulkan_implementation,
    uint32_t option);

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_IMPLEMENTATION_H_

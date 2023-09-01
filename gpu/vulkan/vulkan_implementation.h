// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_IMPLEMENTATION_H_
#define GPU_VULKAN_VULKAN_IMPLEMENTATION_H_

#include <vulkan/vulkan_core.h>

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "build/build_config.h"
#include "gpu/vulkan/semaphore_handle.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_hardware_buffer_handle.h"
#endif

namespace gfx {
class GpuFence;
struct GpuMemoryBufferHandle;
}  // namespace gfx

namespace gpu {
class VulkanDeviceQueue;
class VulkanSurface;
class VulkanImage;
class VulkanInstance;
struct GPUInfo;
struct VulkanYCbCrInfo;

// Base class which provides functions for creating vulkan objects for different
// platforms that use platform-specific extensions (e.g. for creation of
// VkSurfaceKHR objects). It also provides helper/utility functions.
class COMPONENT_EXPORT(VULKAN) VulkanImplementation {
 public:
  explicit VulkanImplementation(bool use_swiftshader = false,
                                bool allow_protected_memory = false);

  VulkanImplementation(const VulkanImplementation&) = delete;
  VulkanImplementation& operator=(const VulkanImplementation&) = delete;

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
  virtual std::vector<const char*> GetOptionalDeviceExtensions() = 0;

  // Creates a VkFence that is exportable to a gfx::GpuFence.
  virtual VkFence CreateVkFenceForGpuFence(VkDevice vk_device) = 0;

  // Exports a VkFence to a gfx::GpuFence.
  //
  // The fence should have been created via CreateVkFenceForGpuFence().
  virtual std::unique_ptr<gfx::GpuFence> ExportVkFenceToGpuFence(
      VkDevice vk_device,
      VkFence vk_fence) = 0;

  // Creates a semaphore that can be exported using GetSemaphoreHandle().
  virtual VkSemaphore CreateExternalSemaphore(VkDevice vk_device);

  // Import a VkSemaphore from a platform-specific handle.
  // Handle types that don't allow permanent import are imported with
  // temporary permanence (VK_SEMAPHORE_IMPORT_TEMPORARY_BIT).
  virtual VkSemaphore ImportSemaphoreHandle(VkDevice vk_device,
                                            SemaphoreHandle handle);

  // Export a platform-specific handle for a Vulkan semaphore. Returns a null
  // handle in case of a failure.
  virtual SemaphoreHandle GetSemaphoreHandle(VkDevice vk_device,
                                             VkSemaphore vk_semaphore);

  // Returns VkExternalSemaphoreHandleTypeFlagBits that should be used when
  // creating and exporting external semaphores.
  virtual VkExternalSemaphoreHandleTypeFlagBits
  GetExternalSemaphoreHandleType() = 0;

  // Returns true if the GpuMemoryBuffer of the specified type can be imported
  // into VkImage using CreateImageFromGpuMemoryHandle().
  virtual bool CanImportGpuMemoryBuffer(
      VulkanDeviceQueue* device_queue,
      gfx::GpuMemoryBufferType memory_buffer_type) = 0;

  // Creates a VkImage from a GpuMemoryBuffer. Implementation must verify that
  // the specified |size| fits in the size specified when |gmb_handle| was
  // allocated.
  virtual std::unique_ptr<VulkanImage> CreateImageFromGpuMemoryHandle(
      VulkanDeviceQueue* device_queue,
      gfx::GpuMemoryBufferHandle gmb_handle,
      gfx::Size size,
      VkFormat vk_format,
      const gfx::ColorSpace& color_space) = 0;

  // Returns whether external semaphores are supported by this device.
  virtual bool IsExternalSemaphoreSupported(VulkanDeviceQueue* device_queue);

#if BUILDFLAG(IS_ANDROID)
  // Get the sampler ycbcr conversion information from the AHB.
  virtual bool GetSamplerYcbcrConversionInfo(
      const VkDevice& vk_device,
      base::android::ScopedHardwareBufferHandle ahb_handle,
      VulkanYCbCrInfo* ycbcr_info) = 0;
#endif

#if BUILDFLAG(IS_FUCHSIA)
  // Registers a sysmem buffer collection. `service_handle` contains a handle
  // for the eventpair that controls the lifetime of the collection. The
  // implementation must drop the collection when all peer handles for
  // that eventpair are destroyed (i.e. when `ZX_EVENTPAIR_PEER_CLOSED` is
  // signaled on that handle). Once a collection is registered the individual
  // buffers in the collection can be referenced by using the peer of
  // `service_handle` as `buffer_collection_handle` in `gmb_handle` passed to
  // CreateImageFromGpuMemoryHandle().
  virtual void RegisterSysmemBufferCollection(
      VkDevice device,
      zx::eventpair service_handle,
      zx::channel sysmem_token,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gfx::Size size,
      size_t min_buffer_count,
      bool register_with_image_pipe) = 0;
#endif  // BUILDFLAG(IS_FUCHSIA)

  bool use_swiftshader() const { return use_swiftshader_; }
  bool allow_protected_memory() const { return allow_protected_memory_; }

 private:
  const bool use_swiftshader_;
  const bool allow_protected_memory_;
};

COMPONENT_EXPORT(VULKAN)
std::unique_ptr<VulkanDeviceQueue> CreateVulkanDeviceQueue(
    VulkanImplementation* vulkan_implementation,
    uint32_t option,
    const GPUInfo* gpu_info = nullptr,
    uint32_t heap_memory_limit = 0,
    const bool is_thread_safe = false);

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_IMPLEMENTATION_H_

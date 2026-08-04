// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_LINUX_OPENXR_VULKAN_CONTEXT_H_
#define DEVICE_VR_OPENXR_LINUX_OPENXR_VULKAN_CONTEXT_H_

#include "base/files/scoped_file.h"
#include "base/scoped_native_library.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/vr_export.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
struct NativePixmapHandle;
}  // namespace gfx

namespace device {

// The Vulkan entry points OpenXrVulkanContext calls, resolved once during
// Initialize(). The core ones must all resolve; the FD-extension ones may stay
// null (guard on has_external_memory_fd()/has_external_semaphore_fd()).
struct VulkanFunctions {
#define DEVICE_VR_OPENXR_VK_DECLARE(fn) PFN_##fn fn = nullptr;
  // Instance-level, required.
  DEVICE_VR_OPENXR_VK_DECLARE(vkEnumerateDeviceExtensionProperties)
  DEVICE_VR_OPENXR_VK_DECLARE(vkGetPhysicalDeviceMemoryProperties)
  DEVICE_VR_OPENXR_VK_DECLARE(vkGetPhysicalDeviceProperties)
  DEVICE_VR_OPENXR_VK_DECLARE(vkGetPhysicalDeviceQueueFamilyProperties)
  // Device-level, required.
  DEVICE_VR_OPENXR_VK_DECLARE(vkAllocateCommandBuffers)
  DEVICE_VR_OPENXR_VK_DECLARE(vkAllocateMemory)
  DEVICE_VR_OPENXR_VK_DECLARE(vkBeginCommandBuffer)
  DEVICE_VR_OPENXR_VK_DECLARE(vkBindImageMemory)
  DEVICE_VR_OPENXR_VK_DECLARE(vkCmdCopyImage)
  DEVICE_VR_OPENXR_VK_DECLARE(vkCmdPipelineBarrier)
  DEVICE_VR_OPENXR_VK_DECLARE(vkCreateCommandPool)
  DEVICE_VR_OPENXR_VK_DECLARE(vkCreateFence)
  DEVICE_VR_OPENXR_VK_DECLARE(vkCreateImage)
  DEVICE_VR_OPENXR_VK_DECLARE(vkCreateSemaphore)
  DEVICE_VR_OPENXR_VK_DECLARE(vkDestroyCommandPool)
  DEVICE_VR_OPENXR_VK_DECLARE(vkDestroyFence)
  DEVICE_VR_OPENXR_VK_DECLARE(vkDestroyImage)
  DEVICE_VR_OPENXR_VK_DECLARE(vkDestroySemaphore)
  DEVICE_VR_OPENXR_VK_DECLARE(vkEndCommandBuffer)
  DEVICE_VR_OPENXR_VK_DECLARE(vkFreeMemory)
  DEVICE_VR_OPENXR_VK_DECLARE(vkGetDeviceQueue)
  DEVICE_VR_OPENXR_VK_DECLARE(vkGetImageMemoryRequirements)
  DEVICE_VR_OPENXR_VK_DECLARE(vkGetImageSubresourceLayout)
  DEVICE_VR_OPENXR_VK_DECLARE(vkQueueSubmit)
  DEVICE_VR_OPENXR_VK_DECLARE(vkResetCommandBuffer)
  DEVICE_VR_OPENXR_VK_DECLARE(vkResetFences)
  DEVICE_VR_OPENXR_VK_DECLARE(vkWaitForFences)
  // Device-level, optional (VK_KHR_external_memory_fd /
  // VK_KHR_external_semaphore_fd).
  DEVICE_VR_OPENXR_VK_DECLARE(vkGetMemoryFdKHR)
  DEVICE_VR_OPENXR_VK_DECLARE(vkGetMemoryFdPropertiesKHR)
  DEVICE_VR_OPENXR_VK_DECLARE(vkImportSemaphoreFdKHR)
#undef DEVICE_VR_OPENXR_VK_DECLARE
};

// Owns the Vulkan instance/device/queue plus the image create/import/copy
// logic, isolating all raw Vulkan calls from OpenXrGraphicsBindingVulkan.
// Compare D3D11TextureHelper.
class DEVICE_VR_EXPORT OpenXrVulkanContext {
 public:
  // Intermediate Vulkan image created by Chromium for SharedImage export. The
  // spec exposes no VkDeviceMemory for runtime-owned swapchain images, so we
  // allocate our own exportable ones and blit them to the swapchain.
  struct IntermediateImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t stride = 0;           // Row pitch from VkSubresourceLayout
    uint32_t offset = 0;           // Plane offset
    uint64_t allocation_size = 0;  // Total allocation size
    // Tracks the current Vulkan image layout so CopyToSwapchainImage can
    // supply the correct oldLayout in pipeline barriers across frames.
    VkImageLayout current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  };

  OpenXrVulkanContext();

  OpenXrVulkanContext(const OpenXrVulkanContext&) = delete;
  OpenXrVulkanContext& operator=(const OpenXrVulkanContext&) = delete;

  ~OpenXrVulkanContext();

  // Creates the VkInstance/VkPhysicalDevice/VkDevice (plus the queue and
  // command infrastructure when FD export is supported) via
  // XR_KHR_vulkan_enable2. Cleans up and returns false on failure.
  bool Initialize(XrInstance instance, XrSystemId system);

  bool initialized() const { return initialized_; }

  // True once the VkDevice and its proc-address getter are available, i.e. the
  // image create/import/copy methods below can be used.
  bool valid() const {
    return vk_device_ != VK_NULL_HANDLE && vk_get_device_proc_addr_;
  }

  bool has_external_memory_fd() const { return has_external_memory_fd_; }
  bool has_external_semaphore_fd() const { return has_external_semaphore_fd_; }

  const gfx::Size& max_texture_size() const { return max_texture_size_; }

  // The XrGraphicsBindingVulkan2KHR chained into xrCreateSession. Valid after a
  // successful Initialize().
  const XrGraphicsBindingVulkan2KHR* binding() const { return &binding_; }

  // Creates one exportable (LINEAR, DMA-BUF) VkImage + VkDeviceMemory pair;
  // the fallback when the GPU-process/GBM allocation path is unavailable.
  bool CreateExportableImage(const gfx::Size& size,
                             VkFormat format,
                             VkImageUsageFlags usage,
                             IntermediateImage* out_image);

  // Imports a DMA-BUF into a VkImage CopyToSwapchainImage can read, using the
  // handle's DRM modifier and plane layout. Returns false on any failure.
  bool ImportDmaBufImage(const gfx::NativePixmapHandle& handle,
                         const gfx::Size& size,
                         VkFormat format,
                         VkImageUsageFlags usage,
                         IntermediateImage* out_image);

  // Exports the image's VkDeviceMemory as a DMA-BUF FD; invalid on failure.
  base::ScopedFD ExportImageMemoryFd(const IntermediateImage& image);

  // Destroys the image and frees its memory; safe on already-null handles.
  void DestroyIntermediateImage(IntermediateImage& image);

  // Blocks queue submission until the sync-file FD signals, then waits for the
  // submit to complete. The FD is borrowed (dup'ed internally). Returns true
  // when there is nothing to wait on (no semaphore-FD extension).
  bool WaitOnSyncFd(int fence_fd);

  // Records and submits the src -> dst blit and waits for it. dst must be an
  // OpenXR swapchain image: it is left in COLOR_ATTACHMENT_OPTIMAL as
  // xrReleaseSwapchainImage expects; src returns to GENERAL for the next frame.
  bool CopyToSwapchainImage(IntermediateImage& src,
                            VkImage dst,
                            const gfx::Size& size);

 private:
  // Fills the instance-level entries of fns_ once the VkInstance exists.
  // Returns false if any is missing.
  bool ResolveInstanceFunctions();

  // Fills the device-level entries of fns_ once the VkDevice exists. Returns
  // false if a core entry point is missing; the FD entry points are optional.
  bool ResolveDeviceFunctions();

  // Cleans up the command pool and command buffer.
  void DestroyCommandPool();

  // Cleans up all Vulkan resources (VkDevice, VkInstance) if they were created.
  void DestroyVulkanState();

  XrGraphicsBindingVulkan2KHR binding_{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
  bool initialized_ = false;

  // Vulkan loader library handle.
  base::ScopedNativeLibrary vulkan_loader_;

  // Raw Vulkan handles owned by this object.
  VkInstance vk_instance_ = VK_NULL_HANDLE;
  VkDevice vk_device_ = VK_NULL_HANDLE;

  // Resolved Vulkan function pointers.
  PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr_ = nullptr;
  PFN_vkGetDeviceProcAddr vk_get_device_proc_addr_ = nullptr;
  PFN_vkDestroyInstance vk_destroy_instance_ = nullptr;
  PFN_vkDestroyDevice vk_destroy_device_ = nullptr;

  // Entry points resolved by ResolveInstanceFunctions()/
  // ResolveDeviceFunctions().
  VulkanFunctions fns_;

  // True if VK_KHR_external_memory_fd was successfully enabled at VkDevice
  // creation. False on runtimes (e.g. SwiftShader) that don't support it.
  bool has_external_memory_fd_ = false;

  // True if VK_KHR_external_semaphore_fd was successfully enabled at VkDevice
  // creation.
  bool has_external_semaphore_fd_ = false;

  // Queue, command pool, command buffer, and submit fence for
  // CopyToSwapchainImage blits.
  VkQueue vk_queue_ = VK_NULL_HANDLE;
  VkCommandPool vk_command_pool_ = VK_NULL_HANDLE;
  VkCommandBuffer vk_command_buffer_ = VK_NULL_HANDLE;
  // Reusable fence for GPU-synchronized submit/wait. Replaces per-call
  // vkQueueWaitIdle, scoping the CPU stall to the specific submit.
  VkFence vk_frame_fence_ = VK_NULL_HANDLE;

  // Maximum texture dimensions queried from VkPhysicalDeviceLimits.
  // Defaults to 4096 until Initialize() queries the real value.
  gfx::Size max_texture_size_{4096, 4096};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_LINUX_OPENXR_VULKAN_CONTEXT_H_

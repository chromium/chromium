// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/android/vulkan_implementation_android.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

VulkanImplementationAndroid::VulkanImplementationAndroid() = default;

VulkanImplementationAndroid::~VulkanImplementationAndroid() = default;

bool VulkanImplementationAndroid::InitializeVulkanInstance(bool using_surface) {
  DCHECK(using_surface);
  std::vector<const char*> required_extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME};

  return vulkan_instance_.Initialize(base::FilePath("libvulkan.so"),
                                     required_extensions, {});
}

VulkanInstance* VulkanImplementationAndroid::GetVulkanInstance() {
  return &vulkan_instance_;
}

std::unique_ptr<VulkanSurface> VulkanImplementationAndroid::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  VkSurfaceKHR surface;
  VkAndroidSurfaceCreateInfoKHR surface_create_info = {};
  surface_create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  surface_create_info.window = window;
  VkResult result = vkCreateAndroidSurfaceKHR(
      vulkan_instance_.vk_instance(), &surface_create_info, nullptr, &surface);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateAndroidSurfaceKHR() failed: " << result;
    return nullptr;
  }

  return std::make_unique<VulkanSurface>(vulkan_instance_.vk_instance(), window,
                                         surface);
}

bool VulkanImplementationAndroid::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  // On Android, all physical devices and queue families must be capable of
  // presentation with any native window.
  // As a result there is no Android-specific query for these capabilities.
  return true;
}

std::vector<const char*>
VulkanImplementationAndroid::GetRequiredDeviceExtensions() {
  return {};
}

std::vector<const char*>
VulkanImplementationAndroid::GetOptionalDeviceExtensions() {
  // VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME also requires
  // VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME as per spec.
  return {
      VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
      VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
      VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
      VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
      VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
  };
}

VkFence VulkanImplementationAndroid::CreateVkFenceForGpuFence(
    VkDevice vk_device) {
  NOTREACHED_IN_MIGRATION();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence>
VulkanImplementationAndroid::ExportVkFenceToGpuFence(VkDevice vk_device,
                                                     VkFence vk_fence) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

VkExternalSemaphoreHandleTypeFlagBits
VulkanImplementationAndroid::GetExternalSemaphoreHandleType() {
  return VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
}

bool VulkanImplementationAndroid::CanImportGpuMemoryBuffer(
    VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return false;
}

std::unique_ptr<VulkanImage>
VulkanImplementationAndroid::CreateImageFromGpuMemoryHandle(
    VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    gfx::Size size,
    VkFormat vk_format,
    const gfx::ColorSpace& color_space) {
  // TODO(sergeyu): Move code from CreateVkImageAndImportAHB() here and remove
  // CreateVkImageAndImportAHB().
  NOTIMPLEMENTED();
  return nullptr;
}

bool VulkanImplementationAndroid::GetSamplerYcbcrConversionInfo(
    const VkDevice& vk_device,
    base::android::ScopedHardwareBufferHandle ahb_handle,
    VulkanYCbCrInfo* ycbcr_info) {
  DCHECK(ycbcr_info);

  // To obtain format properties of an Android hardware buffer, include an
  // instance of VkAndroidHardwareBufferFormatPropertiesANDROID in the pNext
  // chain of the VkAndroidHardwareBufferPropertiesANDROID instance passed to
  // vkGetAndroidHardwareBufferPropertiesANDROID.
  VkAndroidHardwareBufferFormatPropertiesANDROID ahb_format_props = {
      VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID};
  VkAndroidHardwareBufferPropertiesANDROID ahb_props = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
      .pNext = &ahb_format_props,
  };

  VkResult result = vkGetAndroidHardwareBufferPropertiesANDROID(
      vk_device, ahb_handle.get(), &ahb_props);
  if (result != VK_SUCCESS) {
    LOG(ERROR)
        << "GetAhbProps: vkGetAndroidHardwareBufferPropertiesANDROID failed : "
        << result;
    return false;
  }

  *ycbcr_info = VulkanYCbCrInfo(
      VK_FORMAT_UNDEFINED, ahb_format_props.externalFormat,
      ahb_format_props.suggestedYcbcrModel,
      ahb_format_props.suggestedYcbcrRange,
      ahb_format_props.suggestedXChromaOffset,
      ahb_format_props.suggestedYChromaOffset, ahb_format_props.formatFeatures);
  return true;
}

}  // namespace gpu

// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/android/vulkan_implementation_android.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_posix_util.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

namespace {
bool GetAhbProps(
    const VkDevice& vk_device,
    AHardwareBuffer* hardware_buffer,
    VkAndroidHardwareBufferFormatPropertiesANDROID* ahb_format_props,
    VkAndroidHardwareBufferPropertiesANDROID* ahb_props) {
  DCHECK(ahb_format_props);
  DCHECK(ahb_props);

  // To obtain format properties of an Android hardware buffer, include an
  // instance of VkAndroidHardwareBufferFormatPropertiesANDROID in the pNext
  // chain of the VkAndroidHardwareBufferPropertiesANDROID instance passed to
  // vkGetAndroidHardwareBufferPropertiesANDROID.
  ahb_format_props->sType =
      VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;
  ahb_format_props->pNext = nullptr;

  ahb_props->sType =
      VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
  ahb_props->pNext = ahb_format_props;

  bool result = vkGetAndroidHardwareBufferPropertiesANDROID(
      vk_device, hardware_buffer, ahb_props);
  if (result != VK_SUCCESS) {
    LOG(ERROR)
        << "GetAhbProps: vkGetAndroidHardwareBufferPropertiesANDROID failed : "
        << result;
    return false;
  }
  return true;
}

VulkanYCbCrInfo GetYcbcrInfoFromBufferProps(
    const VkAndroidHardwareBufferFormatPropertiesANDROID& ahb_format_props) {
  return VulkanYCbCrInfo(VK_FORMAT_UNDEFINED, ahb_format_props.externalFormat,
                         ahb_format_props.suggestedYcbcrModel,
                         ahb_format_props.suggestedYcbcrRange,
                         ahb_format_props.suggestedXChromaOffset,
                         ahb_format_props.suggestedYChromaOffset,
                         ahb_format_props.formatFeatures);
}

}  // namespace

VulkanImplementationAndroid::VulkanImplementationAndroid() = default;

VulkanImplementationAndroid::~VulkanImplementationAndroid() = default;

bool VulkanImplementationAndroid::InitializeVulkanInstance(bool using_surface) {
  DCHECK(using_surface);
  std::vector<const char*> required_extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME};

  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();

  base::NativeLibraryLoadError native_library_load_error;
  vulkan_function_pointers->vulkan_loader_library_ = base::LoadNativeLibrary(
      base::FilePath("libvulkan.so"), &native_library_load_error);
  if (!vulkan_function_pointers->vulkan_loader_library_)
    return false;

  return vulkan_instance_.Initialize(required_extensions, {});
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

  return std::make_unique<VulkanSurface>(vulkan_instance_.vk_instance(),
                                         surface,
                                         false /* use_protected_memory */);
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
  // VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME also requires
  // VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME as per spec.
  return {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
          VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
          VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
          VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
          VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
          VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
          VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
          VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME};
}

VkFence VulkanImplementationAndroid::CreateVkFenceForGpuFence(
    VkDevice vk_device) {
  NOTREACHED();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence>
VulkanImplementationAndroid::ExportVkFenceToGpuFence(VkDevice vk_device,
                                                     VkFence vk_fence) {
  NOTREACHED();
  return nullptr;
}

VkSemaphore VulkanImplementationAndroid::CreateExternalSemaphore(
    VkDevice vk_device) {
  return CreateExternalVkSemaphore(
      vk_device, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT);
}

VkSemaphore VulkanImplementationAndroid::ImportSemaphoreHandle(
    VkDevice vk_device,
    SemaphoreHandle sync_handle) {
  return ImportVkSemaphoreHandlePosix(vk_device, std::move(sync_handle));
}

SemaphoreHandle VulkanImplementationAndroid::GetSemaphoreHandle(
    VkDevice vk_device,
    VkSemaphore vk_semaphore) {
  // VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT specifies a POSIX file
  // descriptor handle to a Linux Sync File or Android Fence object.
  return GetVkSemaphoreHandlePosix(
      vk_device, vk_semaphore, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT);
}

VkExternalMemoryHandleTypeFlagBits
VulkanImplementationAndroid::GetExternalImageHandleType() {
  return VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;
}

bool VulkanImplementationAndroid::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return false;
}

bool VulkanImplementationAndroid::CreateImageFromGpuMemoryHandle(
    VkDevice vk_device,
    gfx::GpuMemoryBufferHandle gmb_handle,
    gfx::Size size,
    VkImage* vk_image,
    VkImageCreateInfo* vk_image_info,
    VkDeviceMemory* vk_device_memory,
    VkDeviceSize* mem_allocation_size,
    base::Optional<VulkanYCbCrInfo>* ycbcr_info) {
  // TODO(sergeyu): Move code from CreateVkImageAndImportAHB() here and remove
  // CreateVkImageAndImportAHB().
  NOTIMPLEMENTED();
  return false;
}

bool VulkanImplementationAndroid::CreateVkImageAndImportAHB(
    const VkDevice& vk_device,
    const VkPhysicalDevice& vk_physical_device,
    const gfx::Size& size,
    base::android::ScopedHardwareBufferHandle ahb_handle,
    VkImage* vk_image,
    VkImageCreateInfo* vk_image_info,
    VkDeviceMemory* vk_device_memory,
    VkDeviceSize* mem_allocation_size,
    VulkanYCbCrInfo* ycbcr_info) {
  DCHECK(ahb_handle.is_valid());
  DCHECK(vk_image);
  DCHECK(vk_image_info);
  DCHECK(vk_device_memory);
  DCHECK(mem_allocation_size);

  // Get the image format properties of an Android hardware buffer.
  VkAndroidHardwareBufferFormatPropertiesANDROID ahb_format_props = {};
  VkAndroidHardwareBufferPropertiesANDROID ahb_props = {};
  if (!GetAhbProps(vk_device, ahb_handle.get(), &ahb_format_props, &ahb_props))
    return false;

  // To create an image with an external format, include an instance of
  // VkExternalFormatANDROID in the pNext chain of VkImageCreateInfo.
  VkExternalFormatANDROID external_format;
  external_format.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
  external_format.pNext = nullptr;

  // If externalFormat is zero, the effect is as if the VkExternalFormatANDROID
  // structure was not present. Otherwise, the image will have the specified
  // external format.
  external_format.externalFormat = 0;

  // If image has an external format, format must be VK_FORMAT_UNDEFINED.
  if (ahb_format_props.format == VK_FORMAT_UNDEFINED) {
    // externalFormat must be 0 or a value returned in the externalFormat member
    // of VkAndroidHardwareBufferFormatPropertiesANDROID by an earlier call to
    // vkGetAndroidHardwareBufferPropertiesANDROID.
    external_format.externalFormat = ahb_format_props.externalFormat;
  }

  // To define a set of external memory handle types that may be used as backing
  // store for an image, add a VkExternalMemoryImageCreateInfo structure to the
  // pNext chain of the VkImageCreateInfo structure.
  VkExternalMemoryImageCreateInfo external_memory_image_info;
  external_memory_image_info.sType =
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  external_memory_image_info.pNext = &external_format;
  external_memory_image_info.handleTypes =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

  // Intended usage of the image.
  VkImageUsageFlags usage_flags = 0;

  // Get the AHB description.
  AHardwareBuffer_Desc ahb_desc = {};
  base::AndroidHardwareBufferCompat::GetInstance().Describe(ahb_handle.get(),
                                                            &ahb_desc);

  // Get Vulkan Image usage flag equivalence of AHB usage.
  if (ahb_desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) {
    usage_flags = usage_flags | VK_IMAGE_USAGE_SAMPLED_BIT |
                  VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  }
  if (ahb_desc.usage & AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT) {
    usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }
  if (ahb_desc.usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT) {
    usage_flags |= VK_IMAGE_CREATE_PROTECTED_BIT;
  }

  // TODO(vikassoni) : AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP is supported from API
  // level 28 which is not part of current android_ndk version in chromium. Add
  // equvalent VK usage later.

  if (!usage_flags) {
    LOG(ERROR) << "No valid usage flags found";
    return false;
  }

  // Find the first set bit to use as memoryTypeIndex.
  uint32_t memory_type_bits = ahb_props.memoryTypeBits;
  int32_t type_index = -1;
  for (uint32_t i = 0; memory_type_bits;
       memory_type_bits = memory_type_bits >> 0x1, ++i) {
    if (memory_type_bits & 0x1) {
      type_index = i;
      break;
    }
  }
  if (type_index == -1) {
    LOG(ERROR) << "No valid memoryTypeIndex found";
    return false;
  }

  // Populate VkImageCreateInfo.
  vk_image_info->sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  vk_image_info->pNext = &external_memory_image_info;
  vk_image_info->flags = 0;
  vk_image_info->imageType = VK_IMAGE_TYPE_2D;
  vk_image_info->format = ahb_format_props.format;
  vk_image_info->extent = {static_cast<uint32_t>(size.width()),
                           static_cast<uint32_t>(size.height()), 1};
  vk_image_info->mipLevels = 1;
  vk_image_info->arrayLayers = 1;
  vk_image_info->samples = VK_SAMPLE_COUNT_1_BIT;
  vk_image_info->tiling = VK_IMAGE_TILING_OPTIMAL;
  vk_image_info->usage = usage_flags;
  vk_image_info->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vk_image_info->queueFamilyIndexCount = 0;
  vk_image_info->pQueueFamilyIndices = 0;
  vk_image_info->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  // Create Vk Image.
  bool result = vkCreateImage(vk_device, vk_image_info, nullptr, vk_image);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkCreateImage failed : " << result;
    return false;
  }

  // To import memory created outside of the current Vulkan instance from an
  // Android hardware buffer, add a VkImportAndroidHardwareBufferInfoANDROID
  // structure to the pNext chain of the VkMemoryAllocateInfo structure.
  VkImportAndroidHardwareBufferInfoANDROID ahb_import_info;
  ahb_import_info.sType =
      VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
  ahb_import_info.pNext = nullptr;
  ahb_import_info.buffer = ahb_handle.get();

  // If the VkMemoryAllocateInfo pNext chain includes a
  // VkMemoryDedicatedAllocateInfo structure, then that structure includes a
  // handle of the sole buffer or image resource that the memory can be bound
  // to.
  VkMemoryDedicatedAllocateInfo dedicated_alloc_info;
  dedicated_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  dedicated_alloc_info.pNext = &ahb_import_info;
  dedicated_alloc_info.image = *vk_image;
  dedicated_alloc_info.buffer = VK_NULL_HANDLE;

  // An instance of the VkMemoryAllocateInfo structure defines a memory import
  // operation.
  VkMemoryAllocateInfo mem_alloc_info;
  mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc_info.pNext = &dedicated_alloc_info;

  // If the parameters define an import operation and the external handle type
  // is VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
  // allocationSize must be the size returned by
  // vkGetAndroidHardwareBufferPropertiesANDROID for the Android hardware
  // buffer.
  mem_alloc_info.allocationSize = ahb_props.allocationSize;
  mem_alloc_info.memoryTypeIndex = type_index;

  // A Vulkan device operates on data in device memory via memory objects that
  // are represented in the API by a VkDeviceMemory handle.
  // Allocate memory.
  result =
      vkAllocateMemory(vk_device, &mem_alloc_info, nullptr, vk_device_memory);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkAllocateMemory failed : " << result;
    vkDestroyImage(vk_device, *vk_image, nullptr);
    return false;
  }

  // Attach memory to the image object.
  result = vkBindImageMemory(vk_device, *vk_image, *vk_device_memory, 0);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkBindImageMemory failed : " << result;
    vkDestroyImage(vk_device, *vk_image, nullptr);
    vkFreeMemory(vk_device, *vk_device_memory, nullptr);
    return false;
  }

  *mem_allocation_size = mem_alloc_info.allocationSize;
  if (ycbcr_info)
    *ycbcr_info = GetYcbcrInfoFromBufferProps(ahb_format_props);
  return true;
}

bool VulkanImplementationAndroid::GetSamplerYcbcrConversionInfo(
    const VkDevice& vk_device,
    base::android::ScopedHardwareBufferHandle ahb_handle,
    VulkanYCbCrInfo* ycbcr_info) {
  DCHECK(ycbcr_info);

  // Get the image format properties of an Android hardware buffer.
  VkAndroidHardwareBufferFormatPropertiesANDROID ahb_format_props = {};
  VkAndroidHardwareBufferPropertiesANDROID ahb_props = {};
  if (!GetAhbProps(vk_device, ahb_handle.get(), &ahb_format_props, &ahb_props))
    return false;

  *ycbcr_info = GetYcbcrInfoFromBufferProps(ahb_format_props);
  return true;
}

}  // namespace gpu

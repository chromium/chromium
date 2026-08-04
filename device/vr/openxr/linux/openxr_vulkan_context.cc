// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/linux/openxr_vulkan_context.h"

#include <unistd.h>

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/posix/eintr_wrapper.h"
#include "device/vr/openxr/openxr_util.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace device {

OpenXrVulkanContext::OpenXrVulkanContext() = default;

OpenXrVulkanContext::~OpenXrVulkanContext() {
  DestroyVulkanState();
}

bool OpenXrVulkanContext::ResolveInstanceFunctions() {
#define DEVICE_VR_OPENXR_VK_LOAD_INSTANCE(fn)             \
  fns_.fn = reinterpret_cast<PFN_##fn>(                   \
      vk_get_instance_proc_addr_(vk_instance_, #fn));     \
  if (!fns_.fn) {                                         \
    DVLOG(1) << __func__ << " failed to resolve " << #fn; \
    return false;                                         \
  }
  DEVICE_VR_OPENXR_VK_LOAD_INSTANCE(vkEnumerateDeviceExtensionProperties)
  DEVICE_VR_OPENXR_VK_LOAD_INSTANCE(vkGetPhysicalDeviceMemoryProperties)
  DEVICE_VR_OPENXR_VK_LOAD_INSTANCE(vkGetPhysicalDeviceProperties)
  DEVICE_VR_OPENXR_VK_LOAD_INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties)
#undef DEVICE_VR_OPENXR_VK_LOAD_INSTANCE

  return true;
}

bool OpenXrVulkanContext::ResolveDeviceFunctions() {
  if (!vk_get_device_proc_addr_) {
    DVLOG(1) << __func__ << " vkGetDeviceProcAddr unavailable";
    return false;
  }

#define DEVICE_VR_OPENXR_VK_LOAD_DEVICE(fn)                                  \
  fns_.fn =                                                                  \
      reinterpret_cast<PFN_##fn>(vk_get_device_proc_addr_(vk_device_, #fn)); \
  if (!fns_.fn) {                                                            \
    DVLOG(1) << __func__ << " failed to resolve " << #fn;                    \
    return false;                                                            \
  }
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkAllocateCommandBuffers)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkAllocateMemory)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkBeginCommandBuffer)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkBindImageMemory)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkCmdCopyImage)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkCmdPipelineBarrier)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkCreateCommandPool)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkCreateFence)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkCreateImage)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkCreateSemaphore)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkDestroyCommandPool)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkDestroyFence)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkDestroyImage)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkDestroySemaphore)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkEndCommandBuffer)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkFreeMemory)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkGetDeviceQueue)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkGetImageMemoryRequirements)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkGetImageSubresourceLayout)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkQueueSubmit)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkResetCommandBuffer)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkResetFences)
  DEVICE_VR_OPENXR_VK_LOAD_DEVICE(vkWaitForFences)
#undef DEVICE_VR_OPENXR_VK_LOAD_DEVICE

  // Extension-provided; absent on drivers without the FD extensions. Callers
  // gate on has_external_memory_fd()/has_external_semaphore_fd() instead.
#define DEVICE_VR_OPENXR_VK_LOAD_OPTIONAL(fn) \
  fns_.fn =                                   \
      reinterpret_cast<PFN_##fn>(vk_get_device_proc_addr_(vk_device_, #fn));
  DEVICE_VR_OPENXR_VK_LOAD_OPTIONAL(vkGetMemoryFdKHR)
  DEVICE_VR_OPENXR_VK_LOAD_OPTIONAL(vkGetMemoryFdPropertiesKHR)
  DEVICE_VR_OPENXR_VK_LOAD_OPTIONAL(vkImportSemaphoreFdKHR)
#undef DEVICE_VR_OPENXR_VK_LOAD_OPTIONAL

  return true;
}

void OpenXrVulkanContext::DestroyCommandPool() {
  if (vk_device_ != VK_NULL_HANDLE) {
    if (vk_frame_fence_ != VK_NULL_HANDLE) {
      fns_.vkDestroyFence(vk_device_, vk_frame_fence_, nullptr);
    }
    if (vk_command_pool_ != VK_NULL_HANDLE) {
      fns_.vkDestroyCommandPool(vk_device_, vk_command_pool_, nullptr);
    }
  }
  vk_frame_fence_ = VK_NULL_HANDLE;
  vk_command_pool_ = VK_NULL_HANDLE;
  vk_command_buffer_ = VK_NULL_HANDLE;  // Freed with the pool.
  // vk_queue_ lifetime is tied to VkDevice; nulled in DestroyVulkanState.
}

void OpenXrVulkanContext::DestroyVulkanState() {
  DestroyCommandPool();
  vk_queue_ = VK_NULL_HANDLE;  // Queue lifetime is tied to VkDevice.
  if (vk_device_ != VK_NULL_HANDLE && vk_destroy_device_) {
    vk_destroy_device_(vk_device_, nullptr);
    vk_device_ = VK_NULL_HANDLE;
  }
  if (vk_instance_ != VK_NULL_HANDLE && vk_destroy_instance_) {
    vk_destroy_instance_(vk_instance_, nullptr);
    vk_instance_ = VK_NULL_HANDLE;
  }
  fns_ = {};
  binding_ = {XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
  initialized_ = false;
}

bool OpenXrVulkanContext::Initialize(XrInstance instance, XrSystemId system) {
  if (initialized_) {
    return true;
  }

  // --- Step 1: Load the Vulkan loader and get vkGetInstanceProcAddr ---------
  if (!vulkan_loader_.is_valid()) {
    base::NativeLibraryLoadError error;
    vulkan_loader_ = base::ScopedNativeLibrary(
        base::LoadNativeLibrary(base::FilePath("libvulkan.so.1"), &error));
    if (!vulkan_loader_.is_valid()) {
      DVLOG(1) << __func__
               << " Failed to load libvulkan.so.1: " << error.ToString();
      return false;
    }
  }

  vk_get_instance_proc_addr_ = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      vulkan_loader_.GetFunctionPointer("vkGetInstanceProcAddr"));
  if (!vk_get_instance_proc_addr_) {
    DVLOG(1) << __func__ << " Failed to resolve vkGetInstanceProcAddr";
    return false;
  }

  // --- Step 2: Resolve XR function pointers ---------------------------------
  // Declares and resolves `PFN_<fn> <fn>`. All four come from the required
  // XR_KHR_vulkan_enable2, so a missing one is fatal.
#define OPENXR_LOAD_FN(fn)                                               \
  PFN_##fn fn = nullptr;                                                 \
  if (XR_FAILED(xrGetInstanceProcAddr(                                   \
          instance, #fn, reinterpret_cast<PFN_xrVoidFunction*>(&fn))) || \
      !fn) {                                                             \
    DVLOG(1) << __func__ << " Failed to resolve " << #fn;                \
    return false;                                                        \
  }

  OPENXR_LOAD_FN(xrGetVulkanGraphicsRequirements2KHR);
  OPENXR_LOAD_FN(xrCreateVulkanInstanceKHR);
  OPENXR_LOAD_FN(xrGetVulkanGraphicsDevice2KHR);
  OPENXR_LOAD_FN(xrCreateVulkanDeviceKHR);

#undef OPENXR_LOAD_FN

  // --- Step 3: Query Vulkan graphics requirements ---------------------------
  XrGraphicsRequirementsVulkan2KHR reqs{
      XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
  if (XR_FAILED(xrGetVulkanGraphicsRequirements2KHR(instance, system, &reqs))) {
    DVLOG(1) << __func__ << " xrGetVulkanGraphicsRequirements2KHR failed";
    return false;
  }

  // --- Step 4: Create VkInstance via xrCreateVulkanInstanceKHR --------------
  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Chromium";
  app_info.apiVersion =
      VK_MAKE_VERSION(XR_VERSION_MAJOR(reqs.minApiVersionSupported),
                      XR_VERSION_MINOR(reqs.minApiVersionSupported), 0);

  VkInstanceCreateInfo vk_instance_ci{};
  vk_instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  vk_instance_ci.pApplicationInfo = &app_info;

  XrVulkanInstanceCreateInfoKHR xr_vk_instance_ci{
      XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
  xr_vk_instance_ci.systemId = system;
  xr_vk_instance_ci.pfnGetInstanceProcAddr = vk_get_instance_proc_addr_;
  xr_vk_instance_ci.vulkanCreateInfo = &vk_instance_ci;
  xr_vk_instance_ci.vulkanAllocator = nullptr;

  VkResult vk_result = VK_ERROR_UNKNOWN;
  XrResult xr_result = xrCreateVulkanInstanceKHR(instance, &xr_vk_instance_ci,
                                                 &vk_instance_, &vk_result);
  if (XR_FAILED(xr_result) || vk_result != VK_SUCCESS) {
    DVLOG(1) << __func__
             << " xrCreateVulkanInstanceKHR failed: xr=" << xr_result
             << " vk=" << vk_result;
    return false;
  }

  // Resolve instance-level destroy function for cleanup.
  vk_destroy_instance_ = reinterpret_cast<PFN_vkDestroyInstance>(
      vk_get_instance_proc_addr_(vk_instance_, "vkDestroyInstance"));

  if (!ResolveInstanceFunctions()) {
    DestroyVulkanState();
    return false;
  }

  // --- Step 5: Pick VkPhysicalDevice via xrGetVulkanGraphicsDevice2KHR ------
  XrVulkanGraphicsDeviceGetInfoKHR get_info{
      XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
  get_info.systemId = system;
  get_info.vulkanInstance = vk_instance_;

  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  if (XR_FAILED(xrGetVulkanGraphicsDevice2KHR(instance, &get_info,
                                              &physical_device)) ||
      physical_device == VK_NULL_HANDLE) {
    DVLOG(1) << __func__ << " xrGetVulkanGraphicsDevice2KHR failed";
    DestroyVulkanState();
    return false;
  }

  // --- Step 5b: Query VkPhysicalDeviceProperties for real limits ------------
  VkPhysicalDeviceProperties props{};
  fns_.vkGetPhysicalDeviceProperties(physical_device, &props);
  max_texture_size_ =
      gfx::Size(static_cast<int>(props.limits.maxImageDimension2D),
                static_cast<int>(props.limits.maxImageDimension2D));
  DVLOG(1) << __func__
           << " maxImageDimension2D=" << props.limits.maxImageDimension2D;

  // --- Step 6: Find a graphics-capable queue family -------------------------
  uint32_t queue_family_count = 0;
  fns_.vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                                &queue_family_count, nullptr);
  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  fns_.vkGetPhysicalDeviceQueueFamilyProperties(
      physical_device, &queue_family_count, queue_families.data());

  uint32_t graphics_queue_family_index = UINT32_MAX;
  for (uint32_t i = 0; i < queue_family_count; ++i) {
    if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      graphics_queue_family_index = i;
      break;
    }
  }
  if (graphics_queue_family_index == UINT32_MAX) {
    DVLOG(1) << __func__ << " No graphics-capable queue family found";
    DestroyVulkanState();
    return false;
  }

  // --- Step 7: Create VkDevice via xrCreateVulkanDeviceKHR ------------------

  // Enumerate supported device extensions so we can filter our request list.
  uint32_t ext_count = 0;
  fns_.vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                            &ext_count, nullptr);
  std::vector<VkExtensionProperties> available_device_extensions(ext_count);
  fns_.vkEnumerateDeviceExtensionProperties(
      physical_device, nullptr, &ext_count, available_device_extensions.data());

  auto IsExtensionSupported = [&](const char* name) -> bool {
    return std::ranges::any_of(
        available_device_extensions, [name](const VkExtensionProperties& ext) {
          return std::string_view(ext.extensionName) == name;
        });
  };

  std::vector<const char*> device_extensions;
  auto MaybeAdd = [&](const char* name) -> bool {
    if (IsExtensionSupported(name)) {
      device_extensions.push_back(name);
      return true;
    }
    DVLOG(1) << "Device extension not available: " << name;
    return false;
  };

  MaybeAdd(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
  MaybeAdd(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
  MaybeAdd(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
  MaybeAdd(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
  has_external_memory_fd_ = MaybeAdd(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
  MaybeAdd("VK_EXT_external_memory_dma_buf");
  MaybeAdd(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
  has_external_semaphore_fd_ =
      MaybeAdd(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
  MaybeAdd(VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME);
  MaybeAdd(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME);

  float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_ci{};
  queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_ci.queueFamilyIndex = graphics_queue_family_index;
  queue_ci.queueCount = 1;
  queue_ci.pQueuePriorities = &queue_priority;

  VkDeviceCreateInfo vk_device_ci{};
  vk_device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  vk_device_ci.queueCreateInfoCount = 1;
  vk_device_ci.pQueueCreateInfos = &queue_ci;
  vk_device_ci.enabledExtensionCount =
      static_cast<uint32_t>(device_extensions.size());
  vk_device_ci.ppEnabledExtensionNames = device_extensions.data();

  XrVulkanDeviceCreateInfoKHR xr_vk_device_ci{
      XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
  xr_vk_device_ci.systemId = system;
  xr_vk_device_ci.pfnGetInstanceProcAddr = vk_get_instance_proc_addr_;
  xr_vk_device_ci.vulkanPhysicalDevice = physical_device;
  xr_vk_device_ci.vulkanCreateInfo = &vk_device_ci;
  xr_vk_device_ci.vulkanAllocator = nullptr;

  vk_result = VK_ERROR_UNKNOWN;
  xr_result = xrCreateVulkanDeviceKHR(instance, &xr_vk_device_ci, &vk_device_,
                                      &vk_result);
  if (XR_FAILED(xr_result) || vk_result != VK_SUCCESS) {
    DVLOG(1) << __func__ << " xrCreateVulkanDeviceKHR failed: xr=" << xr_result
             << " vk=" << vk_result;
    DestroyVulkanState();
    return false;
  }

  // Both are instance-level entry points that operate on the VkDevice;
  // vkGetDeviceProcAddr then resolves the device-level functions.
  vk_get_device_proc_addr_ = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      vk_get_instance_proc_addr_(vk_instance_, "vkGetDeviceProcAddr"));
  vk_destroy_device_ = reinterpret_cast<PFN_vkDestroyDevice>(
      vk_get_instance_proc_addr_(vk_instance_, "vkDestroyDevice"));

  if (!ResolveDeviceFunctions()) {
    DestroyVulkanState();
    return false;
  }

  // --- Step 8: Retrieve the VkQueue and create a command pool ---------------
  // Only needed when has_external_memory_fd_ is true; without it the copy and
  // sync methods early-return, so there is no blit/sync to set up.
  if (has_external_memory_fd_) {
    fns_.vkGetDeviceQueue(vk_device_, graphics_queue_family_index, 0,
                          &vk_queue_);

    VkCommandPoolCreateInfo pool_ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_ci.queueFamilyIndex = graphics_queue_family_index;
    if (fns_.vkCreateCommandPool(vk_device_, &pool_ci, nullptr,
                                 &vk_command_pool_) != VK_SUCCESS) {
      DVLOG(1) << __func__ << " vkCreateCommandPool failed";
      DestroyVulkanState();
      return false;
    }

    VkCommandBufferAllocateInfo cb_ai{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cb_ai.commandPool = vk_command_pool_;
    cb_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_ai.commandBufferCount = 1;
    if (fns_.vkAllocateCommandBuffers(vk_device_, &cb_ai,
                                      &vk_command_buffer_) != VK_SUCCESS) {
      DVLOG(1) << __func__ << " vkAllocateCommandBuffers failed";
      DestroyVulkanState();
      return false;
    }

    // Create a reusable fence for fence-based submit/wait in WaitOnSyncFd and
    // CopyToSwapchainImage, replacing per-call vkQueueWaitIdle.
    VkFenceCreateInfo fence_ci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (fns_.vkCreateFence(vk_device_, &fence_ci, nullptr, &vk_frame_fence_) !=
        VK_SUCCESS) {
      DVLOG(1) << __func__ << " vkCreateFence failed";
      DestroyVulkanState();
      return false;
    }
  }

  // --- Step 9: Populate the graphics binding struct -------------------------
  binding_.instance = vk_instance_;
  binding_.physicalDevice = physical_device;
  binding_.device = vk_device_;
  binding_.queueFamilyIndex = graphics_queue_family_index;
  binding_.queueIndex = 0;

  initialized_ = true;
  return true;
}

bool OpenXrVulkanContext::CreateExportableImage(const gfx::Size& size,
                                                VkFormat format,
                                                VkImageUsageFlags usage,
                                                IntermediateImage* out_image) {
  CHECK(valid());

  // Chain VkExternalMemoryImageCreateInfo to enable DMA-BUF export.
  // DMA-BUF (not OPAQUE_FD) is required for cross-API sharing with GL.
  VkExternalMemoryImageCreateInfo external_image_info{};
  external_image_info.sType =
      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  external_image_info.handleTypes =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

  VkImageCreateInfo image_ci{};
  image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_ci.pNext = &external_image_info;
  image_ci.imageType = VK_IMAGE_TYPE_2D;
  image_ci.format = format;
  image_ci.extent = {static_cast<uint32_t>(size.width()),
                     static_cast<uint32_t>(size.height()), 1};
  image_ci.mipLevels = 1;
  image_ci.arrayLayers = 1;
  image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
  // LINEAR tiling ensures a well-defined memory layout (queryable stride)
  // that the GL-based GPU process can correctly interpret via DMA-BUF.
  image_ci.tiling = VK_IMAGE_TILING_LINEAR;
  image_ci.usage = usage;
  image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkImage image = VK_NULL_HANDLE;
  if (fns_.vkCreateImage(vk_device_, &image_ci, nullptr, &image) !=
      VK_SUCCESS) {
    DLOG(ERROR) << __func__ << ": vkCreateImage failed";
    return false;
  }

  VkMemoryRequirements mem_reqs{};
  fns_.vkGetImageMemoryRequirements(vk_device_, image, &mem_reqs);

  // Find a device-local memory type that satisfies the image requirements.
  VkPhysicalDeviceMemoryProperties mem_props{};
  fns_.vkGetPhysicalDeviceMemoryProperties(binding_.physicalDevice, &mem_props);

  const base::span<const VkMemoryType> memory_types =
      base::span(mem_props.memoryTypes).first(mem_props.memoryTypeCount);
  uint32_t memory_type_index = UINT32_MAX;
  for (uint32_t i = 0; i < memory_types.size(); ++i) {
    if ((mem_reqs.memoryTypeBits & (1u << i)) &&
        (memory_types[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
      memory_type_index = i;
      break;
    }
  }

  if (memory_type_index == UINT32_MAX) {
    DLOG(ERROR) << __func__ << ": No suitable device-local memory type found";
    fns_.vkDestroyImage(vk_device_, image, nullptr);
    return false;
  }

  // Chain VkExportMemoryAllocateInfo to make the memory exportable as DMA-BUF.
  VkExportMemoryAllocateInfo export_alloc_info{};
  export_alloc_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
  export_alloc_info.handleTypes =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.pNext = &export_alloc_info;
  alloc_info.allocationSize = mem_reqs.size;
  alloc_info.memoryTypeIndex = memory_type_index;

  VkDeviceMemory memory = VK_NULL_HANDLE;
  if (fns_.vkAllocateMemory(vk_device_, &alloc_info, nullptr, &memory) !=
      VK_SUCCESS) {
    DLOG(ERROR) << __func__ << ": vkAllocateMemory failed";
    fns_.vkDestroyImage(vk_device_, image, nullptr);
    return false;
  }

  if (fns_.vkBindImageMemory(vk_device_, image, memory, 0) != VK_SUCCESS) {
    DLOG(ERROR) << __func__ << ": vkBindImageMemory failed";
    fns_.vkDestroyImage(vk_device_, image, nullptr);
    fns_.vkFreeMemory(vk_device_, memory, nullptr);
    return false;
  }

  // Query the actual row stride for LINEAR tiling.
  VkImageSubresource subresource{};
  subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subresource.mipLevel = 0;
  subresource.arrayLayer = 0;
  VkSubresourceLayout layout{};
  fns_.vkGetImageSubresourceLayout(vk_device_, image, &subresource, &layout);

  out_image->image = image;
  out_image->memory = memory;
  out_image->stride = static_cast<uint32_t>(layout.rowPitch);
  out_image->offset = static_cast<uint32_t>(layout.offset);
  out_image->allocation_size = layout.size;
  return true;
}

bool OpenXrVulkanContext::ImportDmaBufImage(
    const gfx::NativePixmapHandle& handle,
    const gfx::Size& size,
    VkFormat format,
    VkImageUsageFlags usage,
    IntermediateImage* out_image) {
  CHECK(valid());

  DVLOG(1) << __func__ << ": planes=" << handle.planes.size() << " modifier=0x"
           << std::hex << handle.modifier << std::dec;
  // Multi-plane modifiers are common for renderable GBM allocations (e.g. AMD
  // DCC adds compression-metadata planes). All planes live in the single BO
  // referenced by planes[0].fd at the given offsets (non-disjoint import).
  if (handle.planes.empty() || handle.planes.size() > 4 ||
      !handle.planes[0].fd.is_valid()) {
    DVLOG(1) << __func__ << ": unsupported plane count " << handle.planes.size()
             << " / invalid fd";
    return false;
  }

  if (!fns_.vkGetMemoryFdPropertiesKHR) {
    DVLOG(1) << __func__ << ": VK_KHR_external_memory_fd not available";
    return false;
  }

  // Create the image with the handle's explicit modifier + per-plane layout;
  // each layout uses offset and rowPitch, size/arrayPitch/depthPitch must be 0.
  std::vector<VkSubresourceLayout> plane_layouts(handle.planes.size());
  for (size_t p = 0; p < handle.planes.size(); ++p) {
    plane_layouts[p].offset = handle.planes[p].offset;
    plane_layouts[p].rowPitch = handle.planes[p].stride;
  }

  VkExternalMemoryImageCreateInfo external_info{};
  external_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
  external_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
  VkImageDrmFormatModifierExplicitCreateInfoEXT explicit_info{};
  explicit_info.sType =
      VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
  explicit_info.pNext = &external_info;
  explicit_info.drmFormatModifier = handle.modifier;
  explicit_info.drmFormatModifierPlaneCount =
      static_cast<uint32_t>(plane_layouts.size());
  explicit_info.pPlaneLayouts = plane_layouts.data();

  VkImageCreateInfo image_ci{};
  image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_ci.pNext = &explicit_info;
  image_ci.imageType = VK_IMAGE_TYPE_2D;
  image_ci.format = format;
  image_ci.extent = {static_cast<uint32_t>(size.width()),
                     static_cast<uint32_t>(size.height()), 1};
  image_ci.mipLevels = 1;
  image_ci.arrayLayers = 1;
  image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
  image_ci.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
  image_ci.usage = usage;
  image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkImage image = VK_NULL_HANDLE;
  if (fns_.vkCreateImage(vk_device_, &image_ci, nullptr, &image) !=
      VK_SUCCESS) {
    DVLOG(1) << __func__ << ": vkCreateImage failed";
    return false;
  }

  // Dup the plane FD for the import (the handle keeps ownership of its own
  // copy; Vulkan takes ownership of the dup on a successful import).
  base::ScopedFD import_fd(HANDLE_EINTR(dup(handle.planes[0].fd.get())));
  if (!import_fd.is_valid()) {
    DVLOG(1) << __func__ << ": dup(fd) failed";
    fns_.vkDestroyImage(vk_device_, image, nullptr);
    return false;
  }

  VkMemoryRequirements mem_reqs{};
  fns_.vkGetImageMemoryRequirements(vk_device_, image, &mem_reqs);
  uint32_t type_bits = mem_reqs.memoryTypeBits;
  VkMemoryFdPropertiesKHR fd_props{};
  fd_props.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
  if (fns_.vkGetMemoryFdPropertiesKHR(
          vk_device_, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
          import_fd.get(), &fd_props) == VK_SUCCESS) {
    type_bits &= fd_props.memoryTypeBits;
  }
  VkPhysicalDeviceMemoryProperties mem_props{};
  fns_.vkGetPhysicalDeviceMemoryProperties(binding_.physicalDevice, &mem_props);
  uint32_t memory_type_index = UINT32_MAX;
  for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
    if (type_bits & (1u << i)) {
      memory_type_index = i;
      break;
    }
  }
  if (memory_type_index == UINT32_MAX) {
    DVLOG(1) << __func__ << ": no memory type (reqs=0x" << std::hex
             << mem_reqs.memoryTypeBits << " fd=0x" << fd_props.memoryTypeBits
             << std::dec << ")";
    fns_.vkDestroyImage(vk_device_, image, nullptr);
    return false;
  }

  VkMemoryDedicatedAllocateInfo dedicated{};
  dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
  dedicated.image = image;
  VkImportMemoryFdInfoKHR import_info{};
  import_info.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
  import_info.pNext = &dedicated;
  import_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
  import_info.fd = import_fd.get();
  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.pNext = &import_info;
  alloc_info.allocationSize = mem_reqs.size;
  alloc_info.memoryTypeIndex = memory_type_index;

  VkDeviceMemory memory = VK_NULL_HANDLE;
  if (fns_.vkAllocateMemory(vk_device_, &alloc_info, nullptr, &memory) !=
      VK_SUCCESS) {
    DVLOG(1) << __func__ << ": vkAllocateMemory (import) failed";
    fns_.vkDestroyImage(vk_device_, image, nullptr);
    return false;
  }
  // A successful import transfers FD ownership to Vulkan.
  [[maybe_unused]] int owned_by_vulkan = import_fd.release();
  if (fns_.vkBindImageMemory(vk_device_, image, memory, 0) != VK_SUCCESS) {
    DVLOG(1) << __func__ << ": vkBindImageMemory failed";
    fns_.vkFreeMemory(vk_device_, memory, nullptr);
    fns_.vkDestroyImage(vk_device_, image, nullptr);
    return false;
  }

  out_image->image = image;
  out_image->memory = memory;
  out_image->stride = handle.planes[0].stride;
  out_image->offset = handle.planes[0].offset;
  out_image->allocation_size = mem_reqs.size;
  return true;
}

base::ScopedFD OpenXrVulkanContext::ExportImageMemoryFd(
    const IntermediateImage& image) {
  // The FD owns its own reference to the DMA-BUF; image.image/memory stay
  // valid until DestroyIntermediateImage.
  int fd = -1;
  VkMemoryGetFdInfoKHR fd_info{VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR};
  fd_info.memory = image.memory;
  fd_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
  if (!fns_.vkGetMemoryFdKHR ||
      fns_.vkGetMemoryFdKHR(vk_device_, &fd_info, &fd) != VK_SUCCESS ||
      fd < 0) {
    DLOG(ERROR) << __func__ << ": vkGetMemoryFdKHR failed";
    return base::ScopedFD();
  }
  return base::ScopedFD(fd);
}

void OpenXrVulkanContext::DestroyIntermediateImage(IntermediateImage& image) {
  if (image.image != VK_NULL_HANDLE) {
    fns_.vkDestroyImage(vk_device_, image.image, nullptr);
    image.image = VK_NULL_HANDLE;
  }
  if (image.memory != VK_NULL_HANDLE) {
    fns_.vkFreeMemory(vk_device_, image.memory, nullptr);
    image.memory = VK_NULL_HANDLE;
  }
}

bool OpenXrVulkanContext::WaitOnSyncFd(int fence_fd) {
  if (!has_external_semaphore_fd_) {
    return true;  // Extension not available (e.g. SwiftShader).
  }

  CHECK(fns_.vkImportSemaphoreFdKHR);  // Implied by has_external_semaphore_fd_.

  VkSemaphore semaphore = VK_NULL_HANDLE;
  VkSemaphoreCreateInfo sem_ci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  if (fns_.vkCreateSemaphore(vk_device_, &sem_ci, nullptr, &semaphore) !=
      VK_SUCCESS) {
    DLOG(ERROR) << __func__ << ": vkCreateSemaphore failed";
    return false;
  }

  // vkImportSemaphoreFdKHR takes ownership of the FD on success, so dup() --
  // the caller still owns the original.
  VkImportSemaphoreFdInfoKHR import_info{
      VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR};
  import_info.semaphore = semaphore;
  import_info.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT;
  import_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
  import_info.fd = dup(fence_fd);

  if (fns_.vkImportSemaphoreFdKHR(vk_device_, &import_info) != VK_SUCCESS) {
    close(import_info.fd);
    fns_.vkDestroySemaphore(vk_device_, semaphore, nullptr);
    DLOG(ERROR) << __func__ << ": vkImportSemaphoreFdKHR failed";
    return false;
  }

  // Submit a wait on the imported semaphore so that subsequent GPU work
  // (the blit in CopyToSwapchainImage) does not start until the fence signals.
  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &semaphore;
  submit.pWaitDstStageMask = &wait_stage;

  // Reset and use the frame fence instead of vkQueueWaitIdle, scoping the CPU
  // stall to this specific submit rather than all pending queue work.
  fns_.vkResetFences(vk_device_, 1, &vk_frame_fence_);
  VkResult result = fns_.vkQueueSubmit(vk_queue_, 1, &submit, vk_frame_fence_);
  if (result == VK_SUCCESS) {
    fns_.vkWaitForFences(vk_device_, 1, &vk_frame_fence_, VK_TRUE, UINT64_MAX);
  }

  fns_.vkDestroySemaphore(vk_device_, semaphore, nullptr);
  return result == VK_SUCCESS;
}

bool OpenXrVulkanContext::CopyToSwapchainImage(IntermediateImage& src_image,
                                               VkImage dst,
                                               const gfx::Size& size) {
  VkImage src = src_image.image;
  fns_.vkResetCommandBuffer(vk_command_buffer_, 0);

  VkCommandBufferBeginInfo begin_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (fns_.vkBeginCommandBuffer(vk_command_buffer_, &begin_info) !=
      VK_SUCCESS) {
    return false;
  }

  // Transition src from its tracked layout (UNDEFINED on first use, GENERAL
  // afterwards) to TRANSFER_SRC_OPTIMAL. srcAccessMask=0 is correct because
  // WaitOnSyncFd already serialized the external GL writes on this queue.
  VkImageMemoryBarrier src_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  src_barrier.srcAccessMask = 0;
  src_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  src_barrier.oldLayout = src_image.current_layout;
  src_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  src_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  src_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  src_barrier.image = src;
  src_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  // Transition dst to TRANSFER_DST_OPTIMAL. Acquired swapchain images start in
  // UNDEFINED or COLOR_ATTACHMENT_OPTIMAL; oldLayout UNDEFINED is always safe.
  VkImageMemoryBarrier dst_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  dst_barrier.srcAccessMask = 0;
  dst_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  dst_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  dst_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  dst_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  dst_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  dst_barrier.image = dst;
  dst_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  // Batch both pre-copy transitions into one call; they share the same stages.
  const VkImageMemoryBarrier pre_copy_barriers[] = {src_barrier, dst_barrier};
  fns_.vkCmdPipelineBarrier(vk_command_buffer_,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                            nullptr, 2, pre_copy_barriers);

  // Copy the intermediate image to the swapchain image.
  VkImageCopy region{};
  region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.extent = {static_cast<uint32_t>(size.width()),
                   static_cast<uint32_t>(size.height()), 1};
  fns_.vkCmdCopyImage(vk_command_buffer_, src,
                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  {
    // Transition dst back: TRANSFER_DST_OPTIMAL -> COLOR_ATTACHMENT_OPTIMAL.
    // OpenXR expects COLOR_ATTACHMENT_OPTIMAL for xrReleaseSwapchainImage.
    dst_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dst_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dst_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dst_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    fns_.vkCmdPipelineBarrier(vk_command_buffer_,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                              0, nullptr, 0, nullptr, 1, &dst_barrier);
  }

  // Transition src back to GENERAL so the next GL/DMA-BUF write has a
  // well-defined Vulkan layout on the subsequent frame.
  src_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  src_barrier.dstAccessMask = 0;
  src_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  src_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  fns_.vkCmdPipelineBarrier(vk_command_buffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr,
                            0, nullptr, 1, &src_barrier);

  if (fns_.vkEndCommandBuffer(vk_command_buffer_) != VK_SUCCESS) {
    return false;
  }

  // Submit with the frame fence instead of vkQueueWaitIdle, scoping the
  // CPU stall to this specific submit.
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &vk_command_buffer_;
  fns_.vkResetFences(vk_device_, 1, &vk_frame_fence_);
  VkResult result = fns_.vkQueueSubmit(vk_queue_, 1, &submit, vk_frame_fence_);
  if (result != VK_SUCCESS) {
    return false;
  }
  fns_.vkWaitForFences(vk_device_, 1, &vk_frame_fence_, VK_TRUE, UINT64_MAX);

  // Record the new layout for next frame's barrier.
  src_image.current_layout = VK_IMAGE_LAYOUT_GENERAL;
  return true;
}

}  // namespace device

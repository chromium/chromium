// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_util.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/config/gpu_info.h"  //nogncheck
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_info.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/gfx/linux/drm_util_linux.h"  //nogncheck
#endif

#define GL_NONE 0x00
#define GL_LAYOUT_GENERAL_EXT 0x958D
#define GL_LAYOUT_COLOR_ATTACHMENT_EXT 0x958E
#define GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT 0x958F
#define GL_LAYOUT_DEPTH_STENCIL_READ_ONLY_EXT 0x9590
#define GL_LAYOUT_SHADER_READ_ONLY_EXT 0x9591
#define GL_LAYOUT_TRANSFER_SRC_EXT 0x9592
#define GL_LAYOUT_TRANSFER_DST_EXT 0x9593
#define GL_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_EXT 0x9530
#define GL_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_EXT 0x9531

namespace gpu {

VulkanPhysicalDeviceProperties MakeVulkanPhysicalDeviceProperties(
    const VkPhysicalDeviceProperties& properties) {
  VulkanPhysicalDeviceProperties result;
  result.driver_version = properties.driverVersion;
  result.vendor_id = properties.vendorID;
  result.device_id = properties.deviceID;
  result.device_name = properties.deviceName;
  return result;
}

bool SubmitSignalVkSemaphores(VkQueue vk_queue,
                              const base::span<VkSemaphore>& vk_semaphores,
                              VkFence vk_fence) {
  // Structure specifying a queue submit operation.
  VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit_info.signalSemaphoreCount = vk_semaphores.size();
  submit_info.pSignalSemaphores = vk_semaphores.data();
  const unsigned int submit_count = 1;
  return vkQueueSubmit(vk_queue, submit_count, &submit_info, vk_fence) ==
         VK_SUCCESS;
}

bool SubmitSignalVkSemaphore(VkQueue vk_queue,
                             VkSemaphore vk_semaphore,
                             VkFence vk_fence) {
  return SubmitSignalVkSemaphores(
      vk_queue, UNSAFE_TODO(base::span<VkSemaphore>(&vk_semaphore, 1u)),
      vk_fence);
}

bool SubmitWaitVkSemaphores(VkQueue vk_queue,
                            const base::span<VkSemaphore>& vk_semaphores,
                            VkFence vk_fence) {
  DCHECK(!vk_semaphores.empty());
  std::vector<VkPipelineStageFlags> semaphore_stages(vk_semaphores.size());
  std::fill(semaphore_stages.begin(), semaphore_stages.end(),
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  // Structure specifying a queue submit operation.
  VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit_info.waitSemaphoreCount = vk_semaphores.size();
  submit_info.pWaitSemaphores = vk_semaphores.data();
  submit_info.pWaitDstStageMask = semaphore_stages.data();
  const unsigned int submit_count = 1;
  return vkQueueSubmit(vk_queue, submit_count, &submit_info, vk_fence) ==
         VK_SUCCESS;
}

bool SubmitWaitVkSemaphore(VkQueue vk_queue,
                           VkSemaphore vk_semaphore,
                           VkFence vk_fence) {
  return SubmitWaitVkSemaphores(
      vk_queue, UNSAFE_TODO(base::span<VkSemaphore>(&vk_semaphore, 1u)),
      vk_fence);
}

VkSemaphore CreateExternalVkSemaphore(
    VkDevice vk_device,
    VkExternalSemaphoreHandleTypeFlags handle_types) {
  VkExportSemaphoreCreateInfo export_info = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
      .handleTypes = handle_types,
  };

  VkSemaphoreCreateInfo sem_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &export_info,
  };

  VkSemaphore semaphore = VK_NULL_HANDLE;
  VkResult result =
      vkCreateSemaphore(vk_device, &sem_info, nullptr, &semaphore);

  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "Failed to create VkSemaphore: " << result;
    return VK_NULL_HANDLE;
  }

  return semaphore;
}

std::string VkVersionToString(uint32_t version) {
  return base::StringPrintf("%u.%u.%u", VK_VERSION_MAJOR(version),
                            VK_VERSION_MINOR(version),
                            VK_VERSION_PATCH(version));
}

VkResult CreateGraphicsPipelinesHook(
    VkDevice device,
    VkPipelineCache pipelineCache,
    uint32_t createInfoCount,
    const VkGraphicsPipelineCreateInfo* pCreateInfos,
    const VkAllocationCallbacks* pAllocator,
    VkPipeline* pPipelines) {
  absl::Cleanup uma_runner = [start_time = base::TimeTicks::Now()] {
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
    // Also emitted from DawnPlatform.
    EmitVkCreateGraphicsPipelinesUMA(elapsed);
  };
  TRACE_EVENT0("gpu", "VulkanCreateGraphicsPipelines");
  return vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount,
                                   pCreateInfos, pAllocator, pPipelines);
}

VkResult VulkanQueueSubmitHook(VkQueue queue,
                               uint32_t submitCount,
                               const VkSubmitInfo* pSubmits,
                               VkFence fence) {
  absl::Cleanup uma_runner = [start_time = base::TimeTicks::Now()] {
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
    // Also emitted from DawnPlatform.
    EmitVkQueueSubmitUMA(elapsed);
  };
  TRACE_EVENT0("gpu", "VulkanQueueSubmitHook");
  return vkQueueSubmit(queue, submitCount, pSubmits, fence);
}

VkResult VulkanQueueWaitIdleHook(VkQueue queue) {
  TRACE_EVENT0("gpu", "VulkanQueueWaitIdleHook");
  return vkQueueWaitIdle(queue);
}

VkResult VulkanQueuePresentKHRHook(VkQueue queue,
                                   const VkPresentInfoKHR* pPresentInfo) {
  TRACE_EVENT0("gpu", "VulkanQueuePresentKHRHook");
  return vkQueuePresentKHR(queue, pPresentInfo);
}

bool CheckVulkanCompatibilities(
    const VulkanPhysicalDeviceProperties& device_properties,
    const GPUInfo& gpu_info) {
// Android uses AHB and SyncFD for interop. They are imported into GL with other
// API.
#if !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN)
  constexpr char kMemoryObjectExtension[] = "GL_EXT_memory_object_win32";
  constexpr char kSemaphoreExtension[] = "GL_EXT_semaphore_win32";
#elif BUILDFLAG(IS_FUCHSIA)
  constexpr char kMemoryObjectExtension[] = "GL_ANGLE_memory_object_fuchsia";
  constexpr char kSemaphoreExtension[] = "GL_ANGLE_semaphore_fuchsia";
#else
  constexpr char kMemoryObjectExtension[] = "GL_EXT_memory_object_fd";
  constexpr char kSemaphoreExtension[] = "GL_EXT_semaphore_fd";
#endif

  // If Chrome and ANGLE share the same VkQueue, they can share vulkan
  // resource without those extensions.
  if (!base::FeatureList::IsEnabled(features::kVulkanFromANGLE)) {
    // If both Vulkan and GL are using native GPU (non swiftshader), check
    // necessary extensions for GL and Vulkan interop.
    const auto extensions = gfx::MakeExtensionSet(gpu_info.gl_extensions);
    if (!gfx::HasExtension(extensions, kMemoryObjectExtension) ||
        !gfx::HasExtension(extensions, kSemaphoreExtension)) {
      DLOG(ERROR) << kMemoryObjectExtension << " or " << kSemaphoreExtension
                  << " is not supported.";
      return false;
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return features::CheckVulkanCompatibilities(device_properties);
}

VkImageLayout GLImageLayoutToVkImageLayout(uint32_t layout) {
  switch (layout) {
    case GL_NONE:
      return VK_IMAGE_LAYOUT_UNDEFINED;
    case GL_LAYOUT_GENERAL_EXT:
      return VK_IMAGE_LAYOUT_GENERAL;
    case GL_LAYOUT_COLOR_ATTACHMENT_EXT:
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT:
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case GL_LAYOUT_DEPTH_STENCIL_READ_ONLY_EXT:
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case GL_LAYOUT_SHADER_READ_ONLY_EXT:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case GL_LAYOUT_TRANSFER_SRC_EXT:
      return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case GL_LAYOUT_TRANSFER_DST_EXT:
      return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case GL_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_EXT:
      return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR;
    case GL_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_EXT:
      return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR;
    default:
      break;
  }
  NOTREACHED() << "Invalid image layout " << layout;
}

uint32_t VkImageLayoutToGLImageLayout(VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return GL_NONE;
    case VK_IMAGE_LAYOUT_GENERAL:
      return GL_LAYOUT_GENERAL_EXT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return GL_LAYOUT_COLOR_ATTACHMENT_EXT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      return GL_LAYOUT_DEPTH_STENCIL_READ_ONLY_EXT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return GL_LAYOUT_SHADER_READ_ONLY_EXT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return GL_LAYOUT_TRANSFER_SRC_EXT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return GL_LAYOUT_TRANSFER_DST_EXT;
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR:
      return GL_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_EXT;
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR:
      return GL_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_EXT;
    default:
      NOTREACHED() << "Invalid image layout " << layout;
  }
}

bool IsVkExternalSemaphoreHandleTypeSupported(
    VulkanDeviceQueue* device_queue,
    VkExternalSemaphoreHandleTypeFlagBits handle_type) {
  if (!gfx::HasExtension(device_queue->enabled_extensions(),
#if BUILDFLAG(IS_WIN)
                         VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
#elif BUILDFLAG(IS_POSIX)
                         VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME
#elif BUILDFLAG(IS_FUCHSIA)
                         VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME
#endif
                         )) {
    return false;
  }

  VkPhysicalDevice physical_device = device_queue->GetVulkanPhysicalDevice();

  VkPhysicalDeviceExternalSemaphoreInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
      .handleType = handle_type,
  };

  VkExternalSemaphoreProperties semaphore_properties = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
  };

  vkGetPhysicalDeviceExternalSemaphoreProperties(
      physical_device, &semaphore_info, &semaphore_properties);

  return (semaphore_properties.externalSemaphoreFeatures &
          VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT) &&
         (semaphore_properties.externalSemaphoreFeatures &
          VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT);
}

VkResult QueryVkExternalMemoryProperties(
    VkPhysicalDevice physical_device,
    VkFormat format,
    VkImageType type,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkExternalMemoryHandleTypeFlagBits handle_type,
    VkExternalMemoryProperties* external_memory_properties) {
  VkPhysicalDeviceImageFormatInfo2 format_info_2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
      .format = format,
      .type = type,
      .tiling = tiling,
      .usage = usage,
      .flags = flags,
  };

  VkPhysicalDeviceExternalImageFormatInfo external_info = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
      .handleType = handle_type,
  };
  format_info_2.pNext = &external_info;

  // From the Vulkan spec:
  //   tiling must be VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT if and only if
  //   the pNext chain includes VkPhysicalDeviceImageDrmFormatModifierInfoEXT
  VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifier_info = {
      .sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  if (tiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT) {
    external_info.pNext = &modifier_info;
  }

  VkImageFormatProperties2 image_format_properties_2 = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
  };
  VkExternalImageFormatProperties external_image_format_properties = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
  };
  image_format_properties_2.pNext = &external_image_format_properties;

  VkResult result = vkGetPhysicalDeviceImageFormatProperties2(
      physical_device, &format_info_2, &image_format_properties_2);
  if (result != VK_SUCCESS) {
    return result;
  }

  *external_memory_properties =
      external_image_format_properties.externalMemoryProperties;
  return VK_SUCCESS;
}

std::vector<VkDrmFormatModifierPropertiesEXT>
QueryVkDrmFormatModifierPropertiesEXT(VkPhysicalDevice physical_device,
                                      VkFormat format) {
  VkDrmFormatModifierPropertiesListEXT modifier_list = {
      .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
  };
  VkFormatProperties2 format_props = {
      .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
      .pNext = &modifier_list,
  };
  vkGetPhysicalDeviceFormatProperties2(physical_device, format, &format_props);

  std::vector<VkDrmFormatModifierPropertiesEXT> modifier_props;
  if (modifier_list.drmFormatModifierCount) {
    modifier_props.resize(modifier_list.drmFormatModifierCount);
    modifier_list.pDrmFormatModifierProperties = modifier_props.data();
    vkGetPhysicalDeviceFormatProperties2(physical_device, format,
                                         &format_props);

    DCHECK_EQ(modifier_list.drmFormatModifierCount, modifier_props.size());
  }

  return modifier_props;
}

void EmitVkCreateGraphicsPipelinesUMA(base::TimeDelta sample) {
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "GPU.Vulkan.SkiaContext.vkCreateGraphicsPipelinesUS", sample,
      base::Microseconds(50), base::Microseconds(1'000'000), 50);
}

void EmitVkQueueSubmitUMA(base::TimeDelta sample) {
  // Subsample this metric as it is on critical path and can be called a lot per
  // frame.
  if (base::ShouldRecordSubsampledMetric(0.01)) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "GPU.Vulkan.SkiaContext.vkQueueSubmitUS", sample,
        base::Microseconds(50), base::Microseconds(1'000'000), 50);
  }
}

}  // namespace gpu

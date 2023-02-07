// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_util.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/config/gpu_info.h"  // nogncheck
#include "gpu/config/vulkan_info.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
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

namespace {

#if BUILDFLAG(IS_ANDROID)
int GetEMUIVersion() {
  const auto* build_info = base::android::BuildInfo::GetInstance();
  base::StringPiece manufacturer(build_info->manufacturer());

  // TODO(crbug.com/1096222): check Honor devices as well.
  if (manufacturer != "HUAWEI")
    return -1;

  // Huawei puts EMUI version in the build version incremental.
  // Example: 11.0.0.130C00
  int version = 0;
  if (sscanf(build_info->version_incremental(), "%d.", &version) != 1)
    return -1;

  return version;
}
#endif
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
      vk_queue, base::span<VkSemaphore>(&vk_semaphore, 1u), vk_fence);
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
      vk_queue, base::span<VkSemaphore>(&vk_semaphore, 1u), vk_fence);
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
  base::ScopedClosureRunner uma_runner(base::BindOnce(
      [](base::Time time) {
        UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            "GPU.Vulkan.PipelineCache.vkCreateGraphicsPipelines",
            base::Time::Now() - time, base::Microseconds(100),
            base::Microseconds(50000), 50);
      },
      base::Time::Now()));
  return vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount,
                                   pCreateInfos, pAllocator, pPipelines);
}

VkResult VulkanQueueSubmitHook(VkQueue queue,
                               uint32_t submitCount,
                               const VkSubmitInfo* pSubmits,
                               VkFence fence) {
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

bool CheckVulkanCompabilities(const VulkanInfo& vulkan_info,
                              const GPUInfo& gpu_info,
                              std::string enable_by_device_name) {
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

#if BUILDFLAG(IS_LINUX) && !defined(OZONE_PLATFORM_IS_X11)
  // Vulkan is only supported with X11 on Linux for now.
  return false;
#else
  return true;
#endif
#else   // BUILDFLAG(IS_ANDROID)
  if (vulkan_info.physical_devices.empty())
    return false;

  const auto& device_info = vulkan_info.physical_devices.front();

  auto enable_patterns = base::SplitString(
      enable_by_device_name, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& enable_pattern : enable_patterns) {
    if (base::MatchPattern(device_info.properties.deviceName, enable_pattern))
      return true;
  }

  if (device_info.properties.vendorID == kVendorARM) {
    int emui_version = GetEMUIVersion();
    // TODO(crbug.com/1096222) Display problem with Huawei EMUI < 11 and Honor
    // devices with Mali GPU. The Mali driver version is < 19.0.0.
    if (device_info.properties.driverVersion < VK_MAKE_VERSION(19, 0, 0) &&
        emui_version < 11) {
      return false;
    }

    // Remove "Mali-" prefix.
    base::StringPiece device_name(device_info.properties.deviceName);
    if (!base::StartsWith(device_name, "Mali-")) {
      LOG(ERROR) << "Unexpected device_name " << device_name;
      return false;
    }
    device_name.remove_prefix(5);

    // Remove anything trailing a space (e.g. "G76 MC4" => "G76").
    device_name = device_name.substr(0, device_name.find(" "));

    // Older Mali GPUs are not performant with Vulkan -- this blocks all Utgard
    // gen, Midgard gen, and some Bifrost 1st & 2nd gen.
    std::vector<const char*> slow_gpus = {"2??", "3??", "4??", "T???",
                                          "G31", "G51", "G52"};
    for (base::StringPiece slow_gpu : slow_gpus) {
      if (base::MatchPattern(device_name, slow_gpu))
        return false;
    }
  }

  // https:://crbug.com/1165783: Performance is not yet as good as GL.
  if (device_info.properties.vendorID == kVendorQualcomm) {
    if (device_info.properties.deviceName ==
        base::StringPiece("Adreno (TM) 630"))
      return true;
    return false;
  }

  // https://crbug.com/1122650: Poor performance and untriaged crashes with
  // Imagination GPUs.
  if (device_info.properties.vendorID == kVendorImagination)
    return false;

  return true;
#endif  // BUILDFLAG(IS_ANDROID)
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
  return VK_IMAGE_LAYOUT_UNDEFINED;
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
      return GL_NONE;
  }
}

}  // namespace gpu

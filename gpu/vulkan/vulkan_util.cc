// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_util.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/config/gpu_info.h"  //nogncheck
#include "gpu/config/vulkan_info.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "gpu/config/gpu_finch_features.h"  //nogncheck
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

bool IsDeviceBlocked(base::StringPiece field, base::StringPiece block_list) {
  auto disable_patterns = base::SplitString(
      block_list, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& disable_pattern : disable_patterns) {
    if (base::MatchPattern(field, disable_pattern)) {
      return true;
    }
  }
  return false;
}

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

bool IsBlockedByBuildInfo() {
  const char* kBlockListByHardware = "mt*";
  const char* kBlockListByBrand = "HONOR";
  const char* kBlockListByDevice = "OP4863|OP4883";
  const char* kBlockListByBoard =
      "RM67*|RM68*|k68*|mt6*|oppo67*|oppo68*|QM215|rk30sdk";

  const auto* build_info = base::android::BuildInfo::GetInstance();
  if (IsDeviceBlocked(build_info->hardware(), kBlockListByHardware)) {
    return true;
  }
  if (IsDeviceBlocked(build_info->brand(), kBlockListByBrand)) {
    return true;
  }
  if (IsDeviceBlocked(build_info->device(), kBlockListByDevice)) {
    return true;
  }
  if (IsDeviceBlocked(build_info->board(), kBlockListByBoard)) {
    return true;
  }

  return false;
}

BASE_FEATURE(kVulkanV2, "VulkanV2", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsVulkanV2Enabled(const GPUInfo& gpu_info,
                       base::StringPiece experiment_arm) {
  const auto* build_info = base::android::BuildInfo::GetInstance();
  // We require at least android T deqp test to pass for v2.
  constexpr int32_t kVulkanDEQPAndroidT = 0x07E60301;
  if (build_info->vulkan_deqp_level() < kVulkanDEQPAndroidT) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(kVulkanV2)) {
    return false;
  }

  const base::FeatureParam<std::string> kBlockListByHardware{
      &kVulkanV2, "BlockListByHardware", ""};

  const base::FeatureParam<std::string> kBlockListByBrand{
      &kVulkanV2, "BlockListByBrand", ""};

  const base::FeatureParam<std::string> kBlockListByDevice{
      &kVulkanV2, "BlockListByDevice", ""};

  const base::FeatureParam<std::string> kBlockListByAndroidBuildId{
      &kVulkanV2, "BlockListByAndroidBuildId", ""};

  const base::FeatureParam<std::string> kBlockListByManufacturer{
      &kVulkanV2, "BlockListByManufacturer", ""};

  const base::FeatureParam<std::string> kBlockListByModel{
      &kVulkanV2, "BlockListByModel", ""};

  const base::FeatureParam<std::string> kBlockListByBoard{
      &kVulkanV2, "BlockListByBoard", ""};

  const base::FeatureParam<std::string> kBlockListByAndroidBuildFP{
      &kVulkanV2, "BlockListByAndroidBuildFP", ""};

  const base::FeatureParam<std::string> kBlockListByGLDriver{
      &kVulkanV2, "BlockListByGLDriver", ""};

  const base::FeatureParam<std::string> kBlockListByGLRenderer{
      &kVulkanV2, "BlockListByGLRenderer", ""};

  const base::FeatureParam<std::string> kBlockListByExperimentArm{
      &kVulkanV2, "BlockListByExperimentArm", ""};

  // Check block list against build info.
  if (IsDeviceBlocked(build_info->hardware(), kBlockListByHardware.Get())) {
    return false;
  }
  if (IsDeviceBlocked(build_info->brand(), kBlockListByBrand.Get())) {
    return false;
  }
  if (IsDeviceBlocked(build_info->device(), kBlockListByDevice.Get())) {
    return false;
  }
  if (IsDeviceBlocked(build_info->android_build_id(),
                      kBlockListByAndroidBuildId.Get())) {
    return false;
  }
  if (IsDeviceBlocked(build_info->manufacturer(),
                      kBlockListByManufacturer.Get())) {
    return false;
  }
  if (IsDeviceBlocked(build_info->model(), kBlockListByModel.Get())) {
    return false;
  }
  if (IsDeviceBlocked(build_info->board(), kBlockListByBoard.Get())) {
    return false;
  }
  if (IsDeviceBlocked(build_info->android_build_fp(),
                      kBlockListByAndroidBuildFP.Get())) {
    return false;
  }

  if (IsDeviceBlocked(gpu_info.gl_renderer, kBlockListByGLRenderer.Get())) {
    return false;
  }

  if (IsDeviceBlocked(gpu_info.gpu.driver_version,
                      kBlockListByGLDriver.Get())) {
    return false;
  }

  if (IsDeviceBlocked(experiment_arm, kBlockListByExperimentArm.Get())) {
    return false;
  }

  return true;
}

bool ShouldBypassImaginationBlock(const GPUInfo& gpu_info) {
  return IsVulkanV2Enabled(gpu_info, "Imagination");
}

bool ShouldBypassAdrenoBlock(const GPUInfo& gpu_info) {
  return IsVulkanV2Enabled(gpu_info, "Adreno");
}

bool ShouldBypassMediatekBlock(const GPUInfo& gpu_info) {
  return IsVulkanV2Enabled(gpu_info, "Mediatek");
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
  if (IsBlockedByBuildInfo() && !ShouldBypassMediatekBlock(gpu_info)) {
    return false;
  }

  if (vulkan_info.physical_devices.empty())
    return false;

  const auto& device_info = vulkan_info.physical_devices.front();

  auto enable_patterns = base::SplitString(
      enable_by_device_name, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& enable_pattern : enable_patterns) {
    if (base::MatchPattern(device_info.properties.deviceName, enable_pattern))
      return true;
  }

  const base::FeatureParam<std::string> disable_patterns(
      &features::kVulkan, "disable_by_gl_renderer", "");

  if (IsDeviceBlocked(gpu_info.gl_renderer, disable_patterns.Get())) {
    return false;
  }

  const base::FeatureParam<std::string> disable_driver_patterns(
      &features::kVulkan, "disable_by_gl_driver", "");
  if (IsDeviceBlocked(gpu_info.gpu.driver_version,
                      disable_driver_patterns.Get())) {
    return false;
  }

  if (device_info.properties.vendorID == kVendorARM) {
    int emui_version = GetEMUIVersion();
    // TODO(crbug.com/1096222) Display problem with Huawei EMUI < 11 and Honor
    // devices with Mali GPU. The Mali driver version is < 19.0.0.
    if (device_info.properties.driverVersion < VK_MAKE_VERSION(19, 0, 0) &&
        emui_version < 11) {
      return false;
    }

    // https://crbug.com/1183702
    if (IsDeviceBlocked(gpu_info.gl_renderer, "*Mali-G?? M*") &&
        !ShouldBypassMediatekBlock(gpu_info)) {
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

  if (device_info.properties.vendorID == kVendorQualcomm) {
    // https://crbug.com/1246857
    if (IsDeviceBlocked(gpu_info.gpu.driver_version,
                        "324.0|331.0|334.0|378.0|415.0|420.0|444.0") &&
        !ShouldBypassAdrenoBlock(gpu_info)) {
      return false;
    }

    // https:://crbug.com/1165783: Performance is not yet as good as GL.
    if (device_info.properties.deviceName ==
        base::StringPiece("Adreno (TM) 630")) {
      return true;
    }

    std::vector<const char*> slow_gpus_for_v2 = {
        "Adreno (TM) 2??", "Adreno (TM) 3??", "Adreno (TM) 4??",
        "Adreno (TM) 5??", "Adreno (TM) 61?", "Adreno (TM) 62?",
    };

    const bool is_slow_gpu_for_v2 =
        base::ranges::any_of(slow_gpus_for_v2, [&](const char* pattern) {
          return base::MatchPattern(device_info.properties.deviceName, pattern);
        });

    // Don't run vulkan for old gpus or if we are not in v2.
    if (is_slow_gpu_for_v2 || !ShouldBypassAdrenoBlock(gpu_info)) {
      return false;
    }

    return true;
  }

  // https://crbug.com/1122650: Poor performance and untriaged crashes with
  // Imagination GPUs.
  if (device_info.properties.vendorID == kVendorImagination &&
      !ShouldBypassImaginationBlock(gpu_info)) {
    return false;
  }

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

}  // namespace gpu

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/vulkan/vulkan_util.h"

#include <string_view>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/config/gpu_info.h"  //nogncheck
#include "gpu/config/vulkan_info.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/gfx/buffer_format_util.h"
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

namespace {

#if BUILDFLAG(IS_ANDROID)

bool IsDeviceBlocked(std::string_view field, std::string_view block_list) {
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
  std::string_view manufacturer(build_info->manufacturer());

  // TODO(crbug.com/40136096): check Honor devices as well.
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
BASE_FEATURE(kVulkanV3, "VulkanV3", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDeviceBlockedByFeatureParams(const GPUInfo& gpu_info,
                                    const base::Feature* feature) {
  const auto* build_info = base::android::BuildInfo::GetInstance();

  const base::FeatureParam<std::string> kBlockListByHardware{
      feature, "BlockListByHardware", ""};

  const base::FeatureParam<std::string> kBlockListByBrand{
      feature, "BlockListByBrand", ""};

  const base::FeatureParam<std::string> kBlockListByDevice{
      feature, "BlockListByDevice", ""};

  const base::FeatureParam<std::string> kBlockListByAndroidBuildId{
      feature, "BlockListByAndroidBuildId", ""};

  const base::FeatureParam<std::string> kBlockListByManufacturer{
      feature, "BlockListByManufacturer", ""};

  const base::FeatureParam<std::string> kBlockListByModel{
      feature, "BlockListByModel", ""};

  const base::FeatureParam<std::string> kBlockListByBoard{
      feature, "BlockListByBoard", ""};

  const base::FeatureParam<std::string> kBlockListByAndroidBuildFP{
      feature, "BlockListByAndroidBuildFP", ""};

  const base::FeatureParam<std::string> kBlockListByGLDriver{
      feature, "BlockListByGLDriver", ""};

  const base::FeatureParam<std::string> kBlockListByGLRenderer{
      feature, "BlockListByGLRenderer", ""};

  // Check block list against build info.
  if (IsDeviceBlocked(build_info->hardware(), kBlockListByHardware.Get())) {
    return true;
  }
  if (IsDeviceBlocked(build_info->brand(), kBlockListByBrand.Get())) {
    return true;
  }
  if (IsDeviceBlocked(build_info->device(), kBlockListByDevice.Get())) {
    return true;
  }
  if (IsDeviceBlocked(build_info->android_build_id(),
                      kBlockListByAndroidBuildId.Get())) {
    return true;
  }
  if (IsDeviceBlocked(build_info->manufacturer(),
                      kBlockListByManufacturer.Get())) {
    return true;
  }
  if (IsDeviceBlocked(build_info->model(), kBlockListByModel.Get())) {
    return true;
  }
  if (IsDeviceBlocked(build_info->board(), kBlockListByBoard.Get())) {
    return true;
  }
  if (IsDeviceBlocked(build_info->android_build_fp(),
                      kBlockListByAndroidBuildFP.Get())) {
    return true;
  }

  if (IsDeviceBlocked(gpu_info.gl_renderer, kBlockListByGLRenderer.Get())) {
    return true;
  }

  if (IsDeviceBlocked(gpu_info.gpu.driver_version,
                      kBlockListByGLDriver.Get())) {
    return true;
  }

  return false;
}

bool IsVulkanV2Allowed() {
  const auto* build_info = base::android::BuildInfo::GetInstance();
  // We require at least android T deqp test to pass for v2.
  constexpr int32_t kVulkanDEQPAndroidT = 0x07E60301;
  if (build_info->vulkan_deqp_level() < kVulkanDEQPAndroidT) {
    return false;
  }

  return true;
}

bool IsVulkanV2Enabled(const GPUInfo& gpu_info,
                       std::string_view experiment_arm) {
  if (!IsVulkanV2Allowed()) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(kVulkanV2)) {
    return false;
  }

  if (IsDeviceBlockedByFeatureParams(gpu_info, &kVulkanV2)) {
    return false;
  }

  const base::FeatureParam<std::string> kBlockListByExperimentArm{
      &kVulkanV2, "BlockListByExperimentArm", ""};

  if (IsDeviceBlocked(experiment_arm, kBlockListByExperimentArm.Get())) {
    return false;
  }

  return true;
}

bool ShouldBypassMediatekBlock(const GPUInfo& gpu_info) {
  return IsVulkanV2Enabled(gpu_info, "Mediatek");
}

// Imagination is allowed with V2.
bool IsVulkanV2EnabledForImagination(const GPUInfo& gpu_info) {
  return IsVulkanV2Enabled(gpu_info, "Imagination");
}

// Everything except MediaTek.
bool IsVulkanV1EnabledForMali(const GPUInfo& gpu_info) {
  // https://crbug.com/1183702
  if (IsDeviceBlocked(gpu_info.gl_renderer, "*Mali-G?? M*")) {
    return false;
  }
  return true;
}

// Everything that passed 2022 deQP tests.
bool IsVulkanV2EnabledForMali(const GPUInfo& gpu_info) {
  // For V2 we MediaTek is allowed.
  return ShouldBypassMediatekBlock(gpu_info);
}

// Only Adreno 630 with drivers newer than 444.0
bool IsVulkanV1EnabledForAdreno(
    const GPUInfo& gpu_info,
    const VulkanPhysicalDeviceProperties& device_properties) {
  // https://crbug.com/1246857
  if (IsDeviceBlocked(gpu_info.gpu.driver_version,
                      "324.0|331.0|334.0|378.0|415.0|420.0|444.0")) {
    return false;
  }

  // https:://crbug.com/1165783: Performance is not yet as good as GL.
  return device_properties.device_name == std::string_view("Adreno (TM) 630");
}

// Adreno 630+ and 2022 deQP tests.
bool IsVulkanV2EnabledForAdreno(
    const GPUInfo& gpu_info,
    const VulkanPhysicalDeviceProperties& device_properties) {
  std::vector<const char*> slow_gpus_for_v2 = {
      "Adreno (TM) 2??", "Adreno (TM) 3??", "Adreno (TM) 4??",
      "Adreno (TM) 5??", "Adreno (TM) 61?", "Adreno (TM) 62?",
  };

  const bool is_slow_gpu_for_v2 =
      base::ranges::any_of(slow_gpus_for_v2, [&](const char* pattern) {
        return base::MatchPattern(device_properties.device_name, pattern);
      });

  // Don't run vulkan for old gpus or if we are not in v2.
  return !is_slow_gpu_for_v2 && IsVulkanV2Enabled(gpu_info, "Adreno");
}

// Adreno 610+ and drivers 502+.
bool IsVulkanV3EnabledForAdreno(
    const GPUInfo& gpu_info,
    const VulkanPhysicalDeviceProperties& device_properties) {
  // If IsVulkanV2Allowed(), this device is part of VulkanV2 finch and we should
  // not make decision again. This is to prevent VulkanV2 control group to get
  // Vulkan enabled by getting into VulkanV3 enabled group.
  if (IsVulkanV2Allowed()) {
    return false;
  }

  std::vector<const char*> slow_gpus_for_v3 = {
      "Adreno (TM) 2??",
      "Adreno (TM) 3??",
      "Adreno (TM) 4??",
      "Adreno (TM) 5??",
  };

  const bool is_slow_gpu_for_v3 =
      base::ranges::any_of(slow_gpus_for_v3, [&](const char* pattern) {
        return base::MatchPattern(device_properties.device_name, pattern);
      });

  if (is_slow_gpu_for_v3) {
    return false;
  }

  constexpr uint32_t kMinVersion = 0x801F6000;  // 502.0
  if (device_properties.driver_version < kMinVersion) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(kVulkanV3)) {
    return false;
  }

  if (IsDeviceBlockedByFeatureParams(gpu_info, &kVulkanV3)) {
    return false;
  }

  return true;
}

#endif
}  // namespace

VulkanPhysicalDeviceProperties::VulkanPhysicalDeviceProperties() = default;

VulkanPhysicalDeviceProperties::VulkanPhysicalDeviceProperties(
    const VkPhysicalDeviceProperties& properties)
    : driver_version(properties.driverVersion),
      vendor_id(properties.vendorID),
      device_id(properties.deviceID),
      device_name(properties.deviceName) {}

VulkanPhysicalDeviceProperties::~VulkanPhysicalDeviceProperties() = default;

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
  absl::Cleanup uma_runner = [start_time = base::TimeTicks::Now()] {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "GPU.Vulkan.PipelineCache.vkCreateGraphicsPipelines",
        base::TimeTicks::Now() - start_time, base::Microseconds(100),
        base::Microseconds(50000), 50);
  };
  TRACE_EVENT0("gpu", "VulkanCreateGraphicsPipelines");
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

  if (device_properties.vendor_id == kVendorARM) {
    int emui_version = GetEMUIVersion();
    // TODO(crbug.com/40136096) Display problem with Huawei EMUI < 11 and Honor
    // devices with Mali GPU. The Mali driver version is < 19.0.0.
    if (device_properties.driver_version < VK_MAKE_VERSION(19, 0, 0) &&
        emui_version < 11) {
      return false;
    }

    // Remove "Mali-" prefix.
    std::string_view device_name(device_properties.device_name);
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
    for (std::string_view slow_gpu : slow_gpus) {
      if (base::MatchPattern(device_name, slow_gpu)) {
        return false;
      }
    }

    return IsVulkanV1EnabledForMali(gpu_info) ||
           IsVulkanV2EnabledForMali(gpu_info);
  }

  if (device_properties.vendor_id == kVendorQualcomm) {
    return IsVulkanV1EnabledForAdreno(gpu_info, device_properties) ||
           IsVulkanV2EnabledForAdreno(gpu_info, device_properties) ||
           IsVulkanV3EnabledForAdreno(gpu_info, device_properties);
  }

  // https://crbug.com/1122650: Poor performance and untriaged crashes with
  // Imagination GPUs.
  if (device_properties.vendor_id == kVendorImagination) {
    // Not allowed with V1.
    return IsVulkanV2EnabledForImagination(gpu_info);
  }

  // Some devices implement Vulkan using Swiftshader. We do not want those,
  // because of performance, and stability (crbug.com/1479335).
  if (device_properties.vendor_id == kVendorGoogle &&
      device_properties.device_id == kDeviceSwiftShader) {
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
  NOTREACHED_IN_MIGRATION() << "Invalid image layout " << layout;
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
      NOTREACHED_IN_MIGRATION() << "Invalid image layout " << layout;
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

void PopulateVkDrmFormatsAndModifiers(
    VulkanDeviceQueue* device_queue,
    base::flat_map<uint32_t, std::vector<uint64_t>>&
        drm_formats_and_modifiers) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  for (int i = 0; i <= static_cast<int>(gfx::BufferFormat::LAST); i++) {
    gfx::BufferFormat buffer_format = static_cast<gfx::BufferFormat>(i);
    VkFormat vk_format = gfx::ToVkFormat(buffer_format);
    int fourcc_format = ui::GetFourCCFormatFromBufferFormat(buffer_format);
    if (vk_format == VK_FORMAT_UNDEFINED || fourcc_format == 0) {
      continue;
    }

    std::vector<VkDrmFormatModifierPropertiesEXT> modifier_props =
        QueryVkDrmFormatModifierPropertiesEXT(
            device_queue->GetVulkanPhysicalDevice(), vk_format);
    if (modifier_props.empty()) {
      continue;
    }

    std::vector<uint64_t> modifiers;
    modifiers.reserve(modifier_props.size());
    for (const auto& props : modifier_props) {
      modifiers.push_back(props.drmFormatModifier);
    }
    drm_formats_and_modifiers.emplace(fourcc_format, std::move(modifiers));
  }
#endif
}

}  // namespace gpu

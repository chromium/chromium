// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/compositor_util.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_blocklist.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "gpu/vulkan/buildflags.h"
#include "media/media_buildflags.h"
#include "ui/gl/gl_switches.h"

namespace content {

namespace {

const int kMinRasterThreads = 1;
const int kMaxRasterThreads = 4;

const int kMinMSAASampleCount = 0;

enum class GpuFeatureInfoType { kCurrent, kForHardwareGpu };

struct DisableInfo {
  // The feature being disabled will be listed as a problem with |description|.
  static DisableInfo Problem(const std::string& description) {
    return DisableInfo{true, description};
  }

  // The feature being disabled will not be listed as a problem.
  static DisableInfo NotProblem() { return DisableInfo{false, ""}; }

  bool is_problem;
  std::string description;
};

struct GpuFeatureData {
  std::string name;
  gpu::GpuFeatureStatus status;
  bool disabled;
  DisableInfo disabled_info;
  bool fallback_to_software;
};

bool IsForceGpuRasterizationEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kForceGpuRasterization);
}

gpu::GpuFeatureStatus SafeGetFeatureStatus(
    const gpu::GpuFeatureInfo& gpu_feature_info,
    gpu::GpuFeatureType feature) {
  if (!gpu_feature_info.IsInitialized()) {
    // The GPU process probably crashed during startup, but we can't
    // assert this as the test bots are slow, and recording the crash
    // is racy. Be robust and just say that all features are disabled.
    return gpu::kGpuFeatureStatusDisabled;
  }
  DCHECK(feature >= 0 && feature < gpu::NUMBER_OF_GPU_FEATURE_TYPES);
  return gpu_feature_info.status_values[feature];
}

gpu::GpuFeatureStatus GetGpuCompositingStatus(
    const gpu::GpuFeatureInfo& gpu_feature_info) {
  return SafeGetFeatureStatus(gpu_feature_info,
                              gpu::GPU_FEATURE_TYPE_GPU_COMPOSITING);
}

const GpuFeatureData GetGpuFeatureData(
    const gpu::GpuFeatureInfo& gpu_feature_info,
    size_t index,
    bool* eof) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  const GpuFeatureData kGpuFeatureData[] = {
    {"2d_canvas",
     SafeGetFeatureStatus(gpu_feature_info,
                          gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS),
     command_line.HasSwitch(switches::kDisableAccelerated2dCanvas),
     DisableInfo::Problem(
         "Accelerated 2D canvas is unavailable: either disabled "
         "via blacklist or the command line."),
     true},
    {"gpu_compositing", GetGpuCompositingStatus(gpu_feature_info),
     command_line.HasSwitch(switches::kDisableGpuCompositing),
     DisableInfo::Problem(
         "Gpu compositing has been disabled, either via blacklist, about:flags "
         "or the command line. The browser will fall back to software "
         "compositing and hardware acceleration will be unavailable."),
     true},
    {"webgl",
     SafeGetFeatureStatus(gpu_feature_info,
                          gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL),
     command_line.HasSwitch(switches::kDisableWebGL),
     DisableInfo::Problem(
         "WebGL has been disabled via blacklist or the command line."),
     false},
    {"flash_3d",
     SafeGetFeatureStatus(gpu_feature_info, gpu::GPU_FEATURE_TYPE_FLASH3D),
     command_line.HasSwitch(switches::kDisableFlash3d),
     DisableInfo::Problem("Using 3d in flash has been disabled, either via "
                          "blacklist, about:flags or the command line."),
     true},
    {"flash_stage3d",
     SafeGetFeatureStatus(gpu_feature_info,
                          gpu::GPU_FEATURE_TYPE_FLASH_STAGE3D),
     command_line.HasSwitch(switches::kDisableFlashStage3d),
     DisableInfo::Problem(
         "Using Stage3d in Flash has been disabled, either via blacklist, "
         "about:flags or the command line."),
     true},
    {"flash_stage3d_baseline",
     SafeGetFeatureStatus(gpu_feature_info,
                          gpu::GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE),
     command_line.HasSwitch(switches::kDisableFlashStage3d),
     DisableInfo::Problem(
         "Using Stage3d Baseline profile in Flash has been disabled, either "
         "via blacklist, about:flags or the command line."),
     true},
    {"protected_video_decode",
     SafeGetFeatureStatus(gpu_feature_info,
                          gpu::GPU_FEATURE_TYPE_PROTECTED_VIDEO_DECODE),
     false,
     DisableInfo::Problem(
         "Protected video decode has been disabled, via blacklist."),
     false},
    {"video_decode",
     SafeGetFeatureStatus(gpu_feature_info,
                          gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE),
     command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode),
     DisableInfo::Problem(
         "Accelerated video decode has been disabled, either via blacklist, "
         "about:flags or the command line."),
     true},
    {"rasterization",
     SafeGetFeatureStatus(gpu_feature_info,
                          gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION),
     (command_line.HasSwitch(switches::kDisableGpuRasterization) &&
      !IsForceGpuRasterizationEnabled()),
     DisableInfo::Problem(
         "Accelerated rasterization has been disabled, either via blacklist, "
         "about:flags or the command line."),
     true},
    {"oop_rasterization",
     SafeGetFeatureStatus(gpu_feature_info,
                          gpu::GPU_FEATURE_TYPE_OOP_RASTERIZATION),
     command_line.HasSwitch(switches::kDisableOopRasterization),
     DisableInfo::NotProblem(), false},
#if defined(OS_MACOSX)
    {"metal",
     SafeGetFeatureStatus(gpu_feature_info, gpu::GPU_FEATURE_TYPE_METAL),
     !base::FeatureList::IsEnabled(features::kMetal) /* disabled */,
     DisableInfo::NotProblem(), false /* fallback_to_software */},
#endif
#if BUILDFLAG(ENABLE_VULKAN)
    {"vulkan",
     SafeGetFeatureStatus(gpu_feature_info, gpu::GPU_FEATURE_TYPE_VULKAN),
     !base::FeatureList::IsEnabled(features::kVulkan) &&
         !command_line.HasSwitch(switches::kUseVulkan) /* disabled */,
     DisableInfo::NotProblem(), false /* fallback_to_software */},
#endif
    {"multiple_raster_threads", gpu::kGpuFeatureStatusEnabled,
     NumberOfRendererRasterThreads() == 1,
     DisableInfo::Problem("Raster is using a single thread."), false},
#if defined(OS_ANDROID)
    {"surface_control",
     SafeGetFeatureStatus(gpu_feature_info,
                          gpu::GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL),
     !features::IsAndroidSurfaceControlEnabled(),
     DisableInfo::Problem(
         "Surface Control has been disabled by Finch trial or command line."),
     false},
#endif
    {"webgl2",
     SafeGetFeatureStatus(gpu_feature_info,
                          gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL2),
     (command_line.HasSwitch(switches::kDisableWebGL) ||
      command_line.HasSwitch(switches::kDisableWebGL2)),
     DisableInfo::Problem(
         "WebGL2 has been disabled via blacklist or the command line."),
     false},
    {"viz_display_compositor", gpu::kGpuFeatureStatusEnabled,
     !features::IsVizDisplayCompositorEnabled(), DisableInfo::NotProblem(),
     false},
    {"viz_hit_test_surface_layer", gpu::kGpuFeatureStatusEnabled,
     !features::IsVizHitTestingSurfaceLayerEnabled(), DisableInfo::NotProblem(),
     false},
    {"skia_renderer", gpu::kGpuFeatureStatusEnabled,
     !features::IsUsingSkiaRenderer(), DisableInfo::NotProblem(), false},
  };
  DCHECK(index < base::size(kGpuFeatureData));
  *eof = (index == base::size(kGpuFeatureData) - 1);
  return kGpuFeatureData[index];
}

std::unique_ptr<base::DictionaryValue> GetFeatureStatusImpl(
    GpuFeatureInfoType type) {
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  std::string gpu_access_blocked_reason;
  bool gpu_access_blocked =
      !manager->GpuAccessAllowed(&gpu_access_blocked_reason);
  const gpu::GpuFeatureInfo gpu_feature_info =
      type == GpuFeatureInfoType::kCurrent
          ? manager->GetGpuFeatureInfo()
          : manager->GetGpuFeatureInfoForHardwareGpu();

  auto feature_status_dict = std::make_unique<base::DictionaryValue>();

  bool eof = false;
  for (size_t i = 0; !eof; ++i) {
    const GpuFeatureData gpu_feature_data =
        GetGpuFeatureData(gpu_feature_info, i, &eof);
    std::string status;
    // Features undergoing a finch controlled roll out.
    if (gpu_feature_data.name == "viz_display_compositor" ||
        gpu_feature_data.name == "skia_renderer" ||
        gpu_feature_data.name == "viz_hit_test_surface_layer") {
      status = (gpu_feature_data.disabled ? "disabled_off_ok" : "enabled_on");
    } else if (gpu_feature_data.disabled || gpu_access_blocked ||
               gpu_feature_data.status == gpu::kGpuFeatureStatusDisabled) {
      status = "disabled";
      if (gpu_feature_data.fallback_to_software)
        status += "_software";
      else
        status += "_off";
    } else if (gpu_feature_data.status == gpu::kGpuFeatureStatusBlacklisted) {
      status = "unavailable_off";
    } else if (gpu_feature_data.status == gpu::kGpuFeatureStatusSoftware) {
      status = "unavailable_software";
    } else {
      status = "enabled";
      if ((gpu_feature_data.name == "webgl" ||
           gpu_feature_data.name == "webgl2") &&
          (GetGpuCompositingStatus(gpu_feature_info) !=
           gpu::kGpuFeatureStatusEnabled))
        status += "_readback";
      if (gpu_feature_data.name == "rasterization") {
        if (IsForceGpuRasterizationEnabled())
          status += "_force";
      }
      if (gpu_feature_data.name == "multiple_raster_threads") {
        const base::CommandLine& command_line =
            *base::CommandLine::ForCurrentProcess();
        if (command_line.HasSwitch(switches::kNumRasterThreads))
          status += "_force";
        status += "_on";
      }
      if (gpu_feature_data.name == "metal" ||
          gpu_feature_data.name == "vulkan" ||
          gpu_feature_data.name == "surface_control") {
        status += "_on";
      }
    }
    feature_status_dict->SetString(gpu_feature_data.name, status);
  }
  return feature_status_dict;
}

std::unique_ptr<base::ListValue> GetProblemsImpl(GpuFeatureInfoType type) {
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  std::string gpu_access_blocked_reason;
  bool gpu_access_blocked =
      !manager->GpuAccessAllowed(&gpu_access_blocked_reason);
  const gpu::GpuFeatureInfo gpu_feature_info =
      type == GpuFeatureInfoType::kCurrent
          ? manager->GetGpuFeatureInfo()
          : manager->GetGpuFeatureInfoForHardwareGpu();

  auto problem_list = std::make_unique<base::ListValue>();
  if (!gpu_feature_info.applied_gpu_blacklist_entries.empty()) {
    std::unique_ptr<gpu::GpuBlocklist> blacklist(gpu::GpuBlocklist::Create());
    blacklist->GetReasons(problem_list.get(), "disabledFeatures",
                          gpu_feature_info.applied_gpu_blacklist_entries);
  }
  if (!gpu_feature_info.applied_gpu_driver_bug_list_entries.empty()) {
    std::unique_ptr<gpu::GpuDriverBugList> bug_list(
        gpu::GpuDriverBugList::Create());
    bug_list->GetReasons(problem_list.get(), "workarounds",
                         gpu_feature_info.applied_gpu_driver_bug_list_entries);
  }

  if (gpu_access_blocked) {
    auto problem = std::make_unique<base::DictionaryValue>();
    problem->SetString("description", "GPU process was unable to boot: " +
                                          gpu_access_blocked_reason);
    problem->Set("crBugs", std::make_unique<base::ListValue>());
    auto disabled_features = std::make_unique<base::ListValue>();
    disabled_features->AppendString("all");
    problem->Set("affectedGpuSettings", std::move(disabled_features));
    problem->SetString("tag", "disabledFeatures");
    problem_list->Insert(0, std::move(problem));
  }

  bool eof = false;
  for (size_t i = 0; !eof; ++i) {
    const GpuFeatureData gpu_feature_data =
        GetGpuFeatureData(gpu_feature_info, i, &eof);
    if (gpu_feature_data.disabled &&
        gpu_feature_data.disabled_info.is_problem) {
      auto problem = std::make_unique<base::DictionaryValue>();
      problem->SetString("description",
                         gpu_feature_data.disabled_info.description);
      problem->Set("crBugs", std::make_unique<base::ListValue>());
      auto disabled_features = std::make_unique<base::ListValue>();
      disabled_features->AppendString(gpu_feature_data.name);
      problem->Set("affectedGpuSettings", std::move(disabled_features));
      problem->SetString("tag", "disabledFeatures");
      problem_list->Append(std::move(problem));
    }
  }
  return problem_list;
}

std::vector<std::string> GetDriverBugWorkaroundsImpl(GpuFeatureInfoType type) {
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  const gpu::GpuFeatureInfo gpu_feature_info =
      type == GpuFeatureInfoType::kCurrent
          ? manager->GetGpuFeatureInfo()
          : manager->GetGpuFeatureInfoForHardwareGpu();

  std::vector<std::string> workarounds;
  for (auto workaround : gpu_feature_info.enabled_gpu_driver_bug_workarounds) {
    workarounds.push_back(gpu::GpuDriverBugWorkaroundTypeToString(
        static_cast<gpu::GpuDriverBugWorkaroundType>(workaround)));
  }
  // Tell clients about the disabled extensions and disabled WebGL
  // extensions as well, to avoid confusion. Do this in a way that's
  // compatible with the current reporting of driver bug workarounds
  // to DevTools and Telemetry, and from there to the GPU tests.
  //
  // This code must be kept in sync with
  // GpuBenchmarking::GetGpuDriverBugWorkarounds.
  for (auto ext : base::SplitString(gpu_feature_info.disabled_extensions,
                                    " ",
                                    base::TRIM_WHITESPACE,
                                    base::SPLIT_WANT_NONEMPTY)) {
    workarounds.push_back("disabled_extension_" + ext);
  }
  for (auto ext : base::SplitString(gpu_feature_info.disabled_webgl_extensions,
                                    " ",
                                    base::TRIM_WHITESPACE,
                                    base::SPLIT_WANT_NONEMPTY)) {
    workarounds.push_back("disabled_webgl_extension_" + ext);
  }
  return workarounds;
}

}  // namespace

int NumberOfRendererRasterThreads() {
  int num_processors = base::SysInfo::NumberOfProcessors();

#if defined(OS_ANDROID) || \
    (defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY))
  // Android and ChromeOS ARM devices may report 6 to 8 CPUs for big.LITTLE
  // configurations. Limit the number of raster threads based on maximum of
  // 4 big cores.
  num_processors = std::min(num_processors, 4);
#endif

  int num_raster_threads = num_processors / 2;

#if defined(OS_ANDROID)
  // Limit the number of raster threads to 1 on Android.
  // TODO(reveman): Remove this when we have a better mechanims to prevent
  // pre-paint raster work from slowing down non-raster work. crbug.com/504515
  num_raster_threads = 1;
#endif

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kNumRasterThreads)) {
    std::string string_value = command_line.GetSwitchValueASCII(
        switches::kNumRasterThreads);
    if (!base::StringToInt(string_value, &num_raster_threads)) {
      DLOG(WARNING) << "Failed to parse switch " <<
          switches::kNumRasterThreads  << ": " << string_value;
    }
  }

  return base::ClampToRange(num_raster_threads, kMinRasterThreads,
                            kMaxRasterThreads);
}

bool IsZeroCopyUploadEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
#if defined(OS_MACOSX)
  return !command_line.HasSwitch(switches::kDisableZeroCopy);
#else
  return command_line.HasSwitch(switches::kEnableZeroCopy);
#endif
}

bool IsPartialRasterEnabled() {
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  return !command_line.HasSwitch(switches::kDisablePartialRaster);
}

bool IsGpuMemoryBufferCompositorResourcesEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(
          switches::kEnableGpuMemoryBufferCompositorResources)) {
    return true;
  }
  if (command_line.HasSwitch(
          switches::kDisableGpuMemoryBufferCompositorResources)) {
    return false;
  }

#if defined(OS_MACOSX)
  return true;
#else
  return false;
#endif
}

int GpuRasterizationMSAASampleCount() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (!command_line.HasSwitch(switches::kGpuRasterizationMSAASampleCount))
#if defined(OS_ANDROID)
    return 4;
#else
    // Desktop platforms will compute this automatically based on DPI.
    return -1;
#endif
  std::string string_value = command_line.GetSwitchValueASCII(
      switches::kGpuRasterizationMSAASampleCount);
  int msaa_sample_count = 0;
  if (base::StringToInt(string_value, &msaa_sample_count) &&
      msaa_sample_count >= kMinMSAASampleCount) {
    return msaa_sample_count;
  } else {
    DLOG(WARNING) << "Failed to parse switch "
                  << switches::kGpuRasterizationMSAASampleCount << ": "
                  << string_value;
    return 0;
  }
}

bool IsMainFrameBeforeActivationEnabled() {
  if (base::SysInfo::NumberOfProcessors() < 4)
    return false;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kDisableMainFrameBeforeActivation))
    return false;

  return true;
}

std::unique_ptr<base::DictionaryValue> GetFeatureStatus() {
  return GetFeatureStatusImpl(GpuFeatureInfoType::kCurrent);
}

std::unique_ptr<base::ListValue> GetProblems() {
  return GetProblemsImpl(GpuFeatureInfoType::kCurrent);
}

std::vector<std::string> GetDriverBugWorkarounds() {
  return GetDriverBugWorkaroundsImpl(GpuFeatureInfoType::kCurrent);
}

std::unique_ptr<base::DictionaryValue> GetFeatureStatusForHardwareGpu() {
  return GetFeatureStatusImpl(GpuFeatureInfoType::kForHardwareGpu);
}

std::unique_ptr<base::ListValue> GetProblemsForHardwareGpu() {
  return GetProblemsImpl(GpuFeatureInfoType::kForHardwareGpu);
}

std::vector<std::string> GetDriverBugWorkaroundsForHardwareGpu() {
  return GetDriverBugWorkaroundsImpl(GpuFeatureInfoType::kForHardwareGpu);
}

}  // namespace content

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/gpu/compositor_util.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "gpu/vulkan/buildflags.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "skia/buildflags.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/ui_base_features.h"
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
  DisableInfo disabled_info = DisableInfo::NotProblem();
  bool fallback_to_software = false;
};

// Returns enabled/disabled based on a bool for when there is no GpuFeatureType.
gpu::GpuFeatureStatus GetFakeFeatureStatus(bool enabled) {
  return enabled ? gpu::kGpuFeatureStatusEnabled
                 : gpu::kGpuFeatureStatusDisabled;
}

// `force_disabled` should only be true when the feature status from
// `gpu_feature_info` is missing information. In general this should be avoided
// and GpuFeatureInfo should be made to be correct.
gpu::GpuFeatureStatus SafeGetFeatureStatus(
    const gpu::GpuFeatureInfo& gpu_feature_info,
    gpu::GpuFeatureType feature,
    bool force_disabled = false) {
  if (force_disabled) {
    return gpu::kGpuFeatureStatusDisabled;
  }

  if (!gpu_feature_info.IsInitialized()) {
    // The GPU process probably crashed during startup, but we can't
    // assert this as the test bots are slow, and recording the crash
    // is racy. Be robust and just say that all features are disabled.
    return gpu::kGpuFeatureStatusDisabled;
  }
  DCHECK(feature >= 0 && feature < gpu::NUMBER_OF_GPU_FEATURE_TYPES);
  return gpu_feature_info.status_values[feature];
}

std::vector<GpuFeatureData> GetGpuFeatureData(
    const gpu::GpuFeatureInfo& gpu_feature_info,
    bool is_gpu_compositing_disabled) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  std::vector<GpuFeatureData> features;
  features.emplace_back(
      "2d_canvas",
      SafeGetFeatureStatus(
          gpu_feature_info, gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
          command_line.HasSwitch(switches::kDisableAccelerated2dCanvas)),
      DisableInfo::Problem(
          "Accelerated 2D canvas is unavailable: either disabled "
          "via blocklist or the command line."),
      true);
  features.emplace_back(
      "canvas_oop_rasterization",
      SafeGetFeatureStatus(
          gpu_feature_info, gpu::GPU_FEATURE_TYPE_CANVAS_OOP_RASTERIZATION,
          command_line.HasSwitch(switches::kDisableAccelerated2dCanvas)));
  features.emplace_back(
      "gpu_compositing",
      // TODO(rivr): Replace with a check to see which backend is used for
      // compositing; do the same for GPU rasterization if it's enabled. For
      // now assume that if GL is blocklisted, then Vulkan is also. Check GL to
      // see if GPU compositing is disabled.
      SafeGetFeatureStatus(gpu_feature_info,
                           gpu::GPU_FEATURE_TYPE_ACCELERATED_GL,
                           is_gpu_compositing_disabled),
      DisableInfo::Problem(
          "Gpu compositing has been disabled, either via blocklist, "
          "about:flags "
          "or the command line. The browser will fall back to software "
          "compositing and hardware acceleration will be unavailable."),
      true);
  features.emplace_back(
      "webgl",
      SafeGetFeatureStatus(gpu_feature_info,
                           gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL,
                           command_line.HasSwitch(switches::kDisableWebGL)),
      DisableInfo::Problem(
          "WebGL has been disabled via blocklist or the command line."),
      false);
  features.emplace_back(
      "video_decode",
      SafeGetFeatureStatus(
          gpu_feature_info, gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE,
#if BUILDFLAG(IS_LINUX)
          !base::FeatureList::IsEnabled(media::kAcceleratedVideoDecodeLinux) ||
#endif  // BUILDFLAG(IS_LINUX)
              command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode)),
      DisableInfo::Problem(
          "Accelerated video decode has been disabled, either via blocklist, "
          "about:flags or the command line."),
      true);
  features.emplace_back(
      "video_encode",
      SafeGetFeatureStatus(
          gpu_feature_info, gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE,
#if BUILDFLAG(IS_LINUX)
          !base::FeatureList::IsEnabled(media::kAcceleratedVideoEncodeLinux)),
#else
          command_line.HasSwitch(switches::kDisableAcceleratedVideoEncode)),
#endif  // BUILDFLAG(IS_LINUX)
      DisableInfo::Problem(
          "Accelerated video encode has been disabled, either via blocklist, "
          "about:flags or the command line."),
      true);
  features.emplace_back(
      "rasterization",
      SafeGetFeatureStatus(gpu_feature_info,
                           gpu::GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION),
      DisableInfo::Problem(
          "Accelerated rasterization has been disabled, either via blocklist, "
          "about:flags or the command line."),
      true);
  features.emplace_back(
      "opengl", SafeGetFeatureStatus(gpu_feature_info,
                                     gpu::GPU_FEATURE_TYPE_ACCELERATED_GL));
#if BUILDFLAG(ENABLE_VULKAN)
  features.emplace_back(
      "vulkan",
      SafeGetFeatureStatus(gpu_feature_info, gpu::GPU_FEATURE_TYPE_VULKAN));
#endif
  features.emplace_back(
      "multiple_raster_threads",
      GetFakeFeatureStatus(NumberOfRendererRasterThreads() > 1));
#if BUILDFLAG(IS_ANDROID)
  features.emplace_back(
      "surface_control",
      SafeGetFeatureStatus(gpu_feature_info,
                           gpu::GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL));
#endif
  features.emplace_back(
      "webgl2",
      SafeGetFeatureStatus(
          gpu_feature_info, gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL2,
          command_line.HasSwitch(switches::kDisableWebGL) ||
              command_line.HasSwitch(switches::kDisableWebGL2)),
      DisableInfo::Problem(
          "WebGL2 has been disabled via blocklist or the command line."),
      false);
  features.emplace_back("raw_draw",
                        GetFakeFeatureStatus(features::IsUsingRawDraw()));
  features.emplace_back("direct_rendering_display_compositor",
                        GetFakeFeatureStatus(features::IsDrDcEnabled()));
  features.emplace_back(
      "webgpu",
      SafeGetFeatureStatus(
          gpu_feature_info, gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGPU,
          !command_line.HasSwitch(switches::kEnableUnsafeWebGPU) &&
              !base::FeatureList::IsEnabled(::features::kWebGPUService)),
      DisableInfo::Problem(
          "WebGPU has been disabled via blocklist or the command line."),
      false);
  features.emplace_back(
      "skia_graphite",
      SafeGetFeatureStatus(gpu_feature_info,
                           gpu::GPU_FEATURE_TYPE_SKIA_GRAPHITE));
  features.emplace_back(
      "webnn",
      SafeGetFeatureStatus(gpu_feature_info, gpu::GPU_FEATURE_TYPE_WEBNN));
  return features;
}

base::Value GetFeatureStatusImpl(GpuFeatureInfoType type) {
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  std::string gpu_access_blocked_reason;
  bool gpu_access_blocked;
  gpu::GpuFeatureInfo gpu_feature_info;
  bool is_gpu_compositing_disabled;
  if (type == GpuFeatureInfoType::kCurrent) {
    gpu_access_blocked = !manager->GpuAccessAllowed(&gpu_access_blocked_reason);
    gpu_feature_info = manager->GetGpuFeatureInfo();
    is_gpu_compositing_disabled = manager->IsGpuCompositingDisabled();
  } else {
    gpu_access_blocked =
        !manager->GpuAccessAllowedForHardwareGpu(&gpu_access_blocked_reason);
    gpu_feature_info = manager->GetGpuFeatureInfoForHardwareGpu();
    is_gpu_compositing_disabled =
        manager->IsGpuCompositingDisabledForHardwareGpu();
  }

  base::Value::Dict feature_status_dict;

  for (auto& gpu_feature_data :
       GetGpuFeatureData(gpu_feature_info, is_gpu_compositing_disabled)) {
    std::string status;
    // Features undergoing a finch controlled roll out.
    if (gpu_feature_data.name == "raw_draw" ||
        gpu_feature_data.name == "direct_rendering_display_compositor") {
      status = gpu_feature_data.status == gpu::kGpuFeatureStatusEnabled
                   ? "enabled_on"
                   : "disabled_off_ok";
    } else if (gpu_access_blocked ||
               gpu_feature_data.status == gpu::kGpuFeatureStatusDisabled) {
      status = "disabled";
      if (gpu_feature_data.fallback_to_software)
        status += "_software";
      else
        status += "_off";
    } else if (gpu_feature_data.status == gpu::kGpuFeatureStatusBlocklisted) {
      status = "unavailable_off";
    } else if (gpu_feature_data.status == gpu::kGpuFeatureStatusSoftware) {
      status = "unavailable_software";
    } else {
      status = "enabled";
      if (gpu_feature_data.name == "canvas_oop_rasterization") {
        status += "_on";
      }
      if ((gpu_feature_data.name == "webgl" ||
           gpu_feature_data.name == "webgl2" ||
           gpu_feature_data.name == "webgpu") &&
          is_gpu_compositing_disabled)
        status += "_readback";
      if (gpu_feature_data.name == "rasterization") {
        const base::CommandLine& command_line =
            *base::CommandLine::ForCurrentProcess();
        if (command_line.HasSwitch(switches::kEnableGpuRasterization))
          status += "_force";
      }
      if (gpu_feature_data.name == "multiple_raster_threads") {
        const base::CommandLine& command_line =
            *base::CommandLine::ForCurrentProcess();
        if (command_line.HasSwitch(cc::switches::kNumRasterThreads)) {
          status += "_force";
        }
        status += "_on";
      }
      if (gpu_feature_data.name == "opengl" ||
          gpu_feature_data.name == "metal" ||
          gpu_feature_data.name == "vulkan" ||
          gpu_feature_data.name == "skia_graphite" ||
          gpu_feature_data.name == "surface_control" ||
          gpu_feature_data.name == "webnn") {
        status += "_on";
      }
    }
    feature_status_dict.Set(gpu_feature_data.name, status);
  }
  return base::Value(std::move(feature_status_dict));
}

base::Value GetProblemsImpl(GpuFeatureInfoType type) {
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  std::string gpu_access_blocked_reason;
  bool gpu_access_blocked;
  gpu::GpuFeatureInfo gpu_feature_info;
  bool is_gpu_compositing_disabled;
  if (type == GpuFeatureInfoType::kCurrent) {
    gpu_access_blocked = !manager->GpuAccessAllowed(&gpu_access_blocked_reason);
    gpu_feature_info = manager->GetGpuFeatureInfo();
    is_gpu_compositing_disabled = manager->IsGpuCompositingDisabled();
  } else {
    gpu_access_blocked =
        !manager->GpuAccessAllowedForHardwareGpu(&gpu_access_blocked_reason);
    gpu_feature_info = manager->GetGpuFeatureInfoForHardwareGpu();
    is_gpu_compositing_disabled =
        manager->IsGpuCompositingDisabledForHardwareGpu();
  }

  base::Value::List problem_list;
  if (!gpu_feature_info.applied_gpu_blocklist_entries.empty()) {
    std::unique_ptr<gpu::GpuBlocklist> blocklist(gpu::GpuBlocklist::Create());
    blocklist->GetReasons(problem_list, "disabledFeatures",
                          gpu_feature_info.applied_gpu_blocklist_entries);
  }
  if (!gpu_feature_info.applied_gpu_driver_bug_list_entries.empty()) {
    std::unique_ptr<gpu::GpuDriverBugList> bug_list(
        gpu::GpuDriverBugList::Create());
    bug_list->GetReasons(problem_list, "workarounds",
                         gpu_feature_info.applied_gpu_driver_bug_list_entries);
  }

  if (gpu_access_blocked) {
    base::Value::Dict problem;
    problem.Set("description",
                "GPU process was unable to boot: " + gpu_access_blocked_reason);
    problem.Set("crBugs", base::Value::List());
    base::Value::List disabled_features;
    disabled_features.Append("all");
    problem.Set("affectedGpuSettings", std::move(disabled_features));
    problem.Set("tag", "disabledFeatures");
    problem_list.Insert(problem_list.begin(), base::Value(std::move(problem)));
  }

  for (auto& gpu_feature_data :
       GetGpuFeatureData(gpu_feature_info, is_gpu_compositing_disabled)) {
    if (gpu_feature_data.status != gpu::kGpuFeatureStatusEnabled &&
        gpu_feature_data.disabled_info.is_problem) {
      base::Value::Dict problem;
      problem.Set("description", gpu_feature_data.disabled_info.description);
      problem.Set("crBugs", base::Value::List());
      base::Value::List disabled_features;
      disabled_features.Append(gpu_feature_data.name);
      problem.Set("affectedGpuSettings", std::move(disabled_features));
      problem.Set("tag", "disabledFeatures");
      problem_list.Insert(problem_list.begin(),
                          base::Value(std::move(problem)));
    }
  }
  return base::Value(std::move(problem_list));
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

#if BUILDFLAG(IS_ANDROID) || \
    (BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY))
  // Android and ChromeOS ARM devices may report 6 to 8 CPUs for big.LITTLE
  // configurations. Limit the number of raster threads based on maximum of
  // 4 big cores.
  num_processors = std::min(num_processors, 4);
#endif

  int num_raster_threads = num_processors / 2;

#if BUILDFLAG(IS_ANDROID)
  // Limit the number of raster threads to 1 on Android.
  // TODO(reveman): Remove this when we have a better mechanims to prevent
  // pre-paint raster work from slowing down non-raster work. crbug.com/504515
  num_raster_threads = 1;
#endif

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(cc::switches::kNumRasterThreads)) {
    std::string string_value =
        command_line.GetSwitchValueASCII(cc::switches::kNumRasterThreads);
    if (!base::StringToInt(string_value, &num_raster_threads)) {
      DLOG(WARNING) << "Failed to parse switch "
                    << cc::switches::kNumRasterThreads << ": " << string_value;
    }
  }

  return std::clamp(num_raster_threads, kMinRasterThreads, kMaxRasterThreads);
}

bool IsZeroCopyUploadEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_MAC)
  return !command_line.HasSwitch(blink::switches::kDisableZeroCopy);
#else
  return command_line.HasSwitch(blink::switches::kEnableZeroCopy);
#endif
}

bool IsPartialRasterEnabled() {
  // Partial raster is not supported with RawDraw.
  if (features::IsUsingRawDraw()) {
    return false;
  }
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  return !command_line.HasSwitch(blink::switches::kDisablePartialRaster);
}

bool IsGpuMemoryBufferCompositorResourcesEnabled() {
  // To use Raw Draw, the Raw Draw shared image backing should be used, so
  // not use GPU memory buffer shared image backings for compositor resources.
  if (features::IsUsingRawDraw()) {
    return false;
  }
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(
          blink::switches::kEnableGpuMemoryBufferCompositorResources)) {
    return true;
  }
  if (command_line.HasSwitch(
          switches::kDisableGpuMemoryBufferCompositorResources)) {
    return false;
  }

#if BUILDFLAG(IS_APPLE)
  return true;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return features::IsDelegatedCompositingEnabled();
#elif BUILDFLAG(IS_WIN)
  return features::IsDelegatedCompositingEnabled() &&
         features::kDelegatedCompositingModeParam.Get() ==
             features::DelegatedCompositingMode::kFull;
#else
  return false;
#endif
}

int GpuRasterizationMSAASampleCount() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (!command_line.HasSwitch(
          blink::switches::kGpuRasterizationMSAASampleCount))
#if BUILDFLAG(IS_ANDROID)
    return 4;
#else
    // Desktop platforms will compute this automatically based on DPI.
    return -1;
#endif
  std::string string_value = command_line.GetSwitchValueASCII(
      blink::switches::kGpuRasterizationMSAASampleCount);
  int msaa_sample_count = 0;
  if (base::StringToInt(string_value, &msaa_sample_count) &&
      msaa_sample_count >= kMinMSAASampleCount) {
    return msaa_sample_count;
  } else {
    DLOG(WARNING) << "Failed to parse switch "
                  << blink::switches::kGpuRasterizationMSAASampleCount << ": "
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

base::Value GetFeatureStatus() {
  return GetFeatureStatusImpl(GpuFeatureInfoType::kCurrent);
}

base::Value GetProblems() {
  return GetProblemsImpl(GpuFeatureInfoType::kCurrent);
}

std::vector<std::string> GetDriverBugWorkarounds() {
  return GetDriverBugWorkaroundsImpl(GpuFeatureInfoType::kCurrent);
}

base::Value GetFeatureStatusForHardwareGpu() {
  return GetFeatureStatusImpl(GpuFeatureInfoType::kForHardwareGpu);
}

base::Value GetProblemsForHardwareGpu() {
  return GetProblemsImpl(GpuFeatureInfoType::kForHardwareGpu);
}

std::vector<std::string> GetDriverBugWorkaroundsForHardwareGpu() {
  return GetDriverBugWorkaroundsImpl(GpuFeatureInfoType::kForHardwareGpu);
}

}  // namespace content

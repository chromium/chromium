// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_util.h"

#include <string_view>

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
// Must be included after windows.h.
#include <psapi.h>
#endif  // BUILDFLAG(IS_WIN)

#include <vulkan/vulkan.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "gpu/config/device_perf_info.h"
#include "gpu/config/gpu_blocklist.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/vulkan/buildflags.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "skia/buildflags.h"
#include "ui/gfx/extension_set.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace gpu {

namespace {

#if BUILDFLAG(IS_WIN)
// These values are persistent to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This should match enum D3D11FeatureLevel in
//  \tools\metrics\histograms\enums.xml
enum class D3D11FeatureLevel {
  kUnknown = 0,
  k9_1 = 4,
  k9_2 = 5,
  k9_3 = 6,
  k10_0 = 7,
  k10_1 = 8,
  k11_0 = 9,
  k11_1 = 10,
  k12_0 = 11,
  k12_1 = 12,
  k12_2 = 13,
  kMaxValue = k12_2,
};

inline D3D11FeatureLevel ConvertToHistogramD3D11FeatureLevel(
    D3D_FEATURE_LEVEL d3d11_feature_level) {
  switch (d3d11_feature_level) {
    case D3D_FEATURE_LEVEL_1_0_CORE:
      return D3D11FeatureLevel::kUnknown;
    case D3D_FEATURE_LEVEL_9_1:
      return D3D11FeatureLevel::k9_1;
    case D3D_FEATURE_LEVEL_9_2:
      return D3D11FeatureLevel::k9_2;
    case D3D_FEATURE_LEVEL_9_3:
      return D3D11FeatureLevel::k9_3;
    case D3D_FEATURE_LEVEL_10_0:
      return D3D11FeatureLevel::k10_0;
    case D3D_FEATURE_LEVEL_10_1:
      return D3D11FeatureLevel::k10_1;
    case D3D_FEATURE_LEVEL_11_0:
      return D3D11FeatureLevel::k11_0;
    case D3D_FEATURE_LEVEL_11_1:
      return D3D11FeatureLevel::k11_1;
    case D3D_FEATURE_LEVEL_12_0:
      return D3D11FeatureLevel::k12_0;
    case D3D_FEATURE_LEVEL_12_1:
      return D3D11FeatureLevel::k12_1;
    case D3D_FEATURE_LEVEL_12_2:
      return D3D11FeatureLevel::k12_2;
    default:
      NOTREACHED_IN_MIGRATION();
      return D3D11FeatureLevel::kUnknown;
  }
}
#endif  // BUILDFLAG(IS_WIN)

GpuFeatureStatus GetAndroidSurfaceControlFeatureStatus(
    const std::set<int>& blocklisted_features,
    const GpuPreferences& gpu_preferences) {
#if !BUILDFLAG(IS_ANDROID)
  return kGpuFeatureStatusDisabled;
#else
  if (!gpu_preferences.enable_android_surface_control)
    return kGpuFeatureStatusDisabled;

  // SurfaceControl as used by Chrome requires using GpuFence for
  // synchronization, this is based on Android native fence sync
  // support. If that is unavailable, i.e. on emulator or SwiftShader,
  // don't claim SurfaceControl support.
  if (!gl::GetDefaultDisplayEGL()->IsAndroidNativeFenceSyncSupported())
    return kGpuFeatureStatusDisabled;

  DCHECK(gfx::SurfaceControl::IsSupported());
  return kGpuFeatureStatusEnabled;
#endif
}

GpuFeatureStatus GetVulkanFeatureStatus(
    const std::set<int>& blocklisted_features,
    const GpuPreferences& gpu_preferences) {
#if BUILDFLAG(ENABLE_VULKAN)
  // Only blocklist native vulkan.
  if (gpu_preferences.use_vulkan == VulkanImplementationName::kNative &&
      blocklisted_features.count(GPU_FEATURE_TYPE_VULKAN))
    return kGpuFeatureStatusBlocklisted;

  if (gpu_preferences.use_vulkan == VulkanImplementationName::kNone)
    return kGpuFeatureStatusDisabled;

  return kGpuFeatureStatusEnabled;
#else
  return kGpuFeatureStatusDisabled;
#endif
}

GpuFeatureStatus GetGpuRasterizationFeatureStatus(
    const std::set<int>& blocklisted_features,
    const base::CommandLine& command_line,
    bool use_swift_shader) {
  if (command_line.HasSwitch(switches::kDisableGpuRasterization))
    return kGpuFeatureStatusDisabled;
  else if (command_line.HasSwitch(switches::kEnableGpuRasterization))
    return kGpuFeatureStatusEnabled;

  // If swiftshader is being used, the blocklist should be ignored.
  if (!use_swift_shader &&
      blocklisted_features.count(GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION)) {
    return kGpuFeatureStatusBlocklisted;
  }

  // Enable gpu rasterization for vulkan, unless it is overridden by
  // commandline.
  if (features::IsUsingVulkan() &&
      !base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          features::kDefaultEnableGpuRasterization.name,
          base::FeatureList::OVERRIDE_DISABLE_FEATURE)) {
    return kGpuFeatureStatusEnabled;
  }

  // Gpu Rasterization on platforms that are not fully enabled is controlled by
  // a finch experiment.
  if (!base::FeatureList::IsEnabled(features::kDefaultEnableGpuRasterization))
    return kGpuFeatureStatusDisabled;

  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetWebGLFeatureStatus(
    const std::set<int>& blocklisted_features,
    bool use_swift_shader) {
  if (use_swift_shader)
    return kGpuFeatureStatusEnabled;
  if (blocklisted_features.count(GPU_FEATURE_TYPE_ACCELERATED_WEBGL))
    return kGpuFeatureStatusBlocklisted;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetWebGL2FeatureStatus(
    const std::set<int>& blocklisted_features,
    bool use_swift_shader) {
  if (use_swift_shader)
    return kGpuFeatureStatusEnabled;
  if (blocklisted_features.count(GPU_FEATURE_TYPE_ACCELERATED_WEBGL2))
    return kGpuFeatureStatusBlocklisted;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetWebGPUFeatureStatus(
    const std::set<int>& blocklisted_features,
    bool use_swift_shader) {
  if (use_swift_shader)
    return kGpuFeatureStatusSoftware;
  if (blocklisted_features.count(GPU_FEATURE_TYPE_ACCELERATED_WEBGPU))
    return kGpuFeatureStatusSoftware;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus Get2DCanvasFeatureStatus(
    const std::set<int>& blocklisted_features,
    bool use_swift_shader) {
  if (use_swift_shader) {
    // This is for testing only. Chrome should exercise the GPU accelerated
    // path on top of SwiftShader driver.
    return kGpuFeatureStatusEnabled;
  }
  if (blocklisted_features.count(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS))
    return kGpuFeatureStatusSoftware;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetCanvasOopRasterizationFeatureStatus(
    const std::set<int>& blocklisted_features,
    const GpuPreferences& gpu_preferences) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Disable OOP-C if explicitly turned off from the command line.
  base::FeatureList* feature_list = base::FeatureList::GetInstance();
  if (feature_list && feature_list->IsFeatureOverriddenFromCommandLine(
                          features::kCanvasOopRasterization.name,
                          base::FeatureList::OVERRIDE_DISABLE_FEATURE)) {
    return kGpuFeatureStatusDisabled;
  }

  // On certain ChromeOS devices, using Vulkan without OOP-C results in video
  // encode artifacts (b/318721705).
  if (gpu_preferences.use_vulkan != VulkanImplementationName::kNone)
    return kGpuFeatureStatusEnabled;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Canvas OOP Rasterization on platforms that are not fully enabled is
  // controlled by a finch experiment.
  if (!features::IsCanvasOopRasterizationEnabled()) {
    return kGpuFeatureStatusDisabled;
  }

  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetAcceleratedVideoDecodeFeatureStatus(
    const std::set<int>& blocklisted_features,
    bool use_swift_shader) {
  if (use_swift_shader) {
    // This is for testing only. Chrome should exercise the GPU accelerated
    // path on top of SwiftShader driver.
    return kGpuFeatureStatusEnabled;
  }
  if (blocklisted_features.count(GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE))
    return kGpuFeatureStatusBlocklisted;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetAcceleratedVideoEncodeFeatureStatus(
    const std::set<int>& blocklisted_features,
    bool use_swift_shader) {
  if (use_swift_shader) {
    // This is for testing only. Chrome should exercise the GPU accelerated
    // path on top of SwiftShader driver.
    return kGpuFeatureStatusEnabled;
  }
  if (blocklisted_features.count(GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE))
    return kGpuFeatureStatusBlocklisted;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetGLFeatureStatus(const std::set<int>& blocklisted_features,
                                    bool use_swift_shader) {
  if (use_swift_shader) {
    // This is for testing only. Chrome should exercise the GPU accelerated
    // path on top of SwiftShader driver.
    return kGpuFeatureStatusEnabled;
  }
  if (blocklisted_features.count(GPU_FEATURE_TYPE_ACCELERATED_GL))
    return kGpuFeatureStatusBlocklisted;
  return kGpuFeatureStatusEnabled;
}

GpuFeatureStatus GetSkiaGraphiteFeatureStatus(
    const std::set<int>& blocklisted_features,
    const GpuPreferences& gpu_preferences) {
  if (blocklisted_features.count(GPU_FEATURE_TYPE_SKIA_GRAPHITE)) {
    return kGpuFeatureStatusDisabled;
  }
#if BUILDFLAG(SKIA_USE_DAWN)
  if (gpu_preferences.gr_context_type == GrContextType::kGraphiteDawn) {
    return kGpuFeatureStatusEnabled;
  }
#endif  // BUILDFLAG(SKIA_USE_DAWN)
#if BUILDFLAG(SKIA_USE_METAL)
  if (gpu_preferences.gr_context_type == GrContextType::kGraphiteMetal) {
    return kGpuFeatureStatusEnabled;
  }
#endif  // BUILDFLAG(SKIA_USE_METAL)
  return kGpuFeatureStatusDisabled;
}

GpuFeatureStatus GetWebNNFeatureStatus(
    const std::set<int>& blocklisted_features) {
  if (!base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebMachineLearningNeuralNetwork)) {
    return kGpuFeatureStatusDisabled;
  }
  if (blocklisted_features.count(GPU_FEATURE_TYPE_WEBNN)) {
    return kGpuFeatureStatusSoftware;
  }
  return kGpuFeatureStatusEnabled;
}

void SetProcessGlWorkaroundsFromGpuFeatures(
    const GpuFeatureInfo& gpu_feature_info) {
  const auto is_enabled =
      [&gpu_feature_info](const gpu::GpuDriverBugWorkaroundType& type) {
        return gpu_feature_info.IsWorkaroundEnabled(type);
      };

  gl::GlWorkarounds workarounds = {
      .disable_d3d11 = is_enabled(DISABLE_D3D11),
      .disable_metal = is_enabled(DISABLE_METAL),
      .disable_es3gl_context = is_enabled(DISABLE_ES3_GL_CONTEXT),
#if BUILDFLAG(IS_WIN)
      .disable_direct_composition = is_enabled(DISABLE_DIRECT_COMPOSITION),
      .disable_direct_composition_video_overlays =
          is_enabled(DISABLE_DIRECT_COMPOSITION_VIDEO_OVERLAYS),
      .disable_vp_auto_hdr = is_enabled(DISABLE_VP_AUTO_HDR),
#endif
  };

  gl::SetGlWorkarounds(workarounds);
}

// Adjust gpu feature status based on enabled gpu driver bug workarounds.
void AdjustGpuFeatureStatusToWorkarounds(GpuFeatureInfo* gpu_feature_info,
                                         const GPUInfo& gpu_info) {
  if (gpu_feature_info->IsWorkaroundEnabled(DISABLE_D3D11) ||
      gpu_feature_info->IsWorkaroundEnabled(DISABLE_ES3_GL_CONTEXT)) {
    gpu_feature_info->status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL2] =
        kGpuFeatureStatusBlocklisted;
  }
  if (gpu_feature_info->IsWorkaroundEnabled(DISABLE_CANVAS_OOP_RASTERIZATION)) {
    gpu_feature_info->status_values[GPU_FEATURE_TYPE_CANVAS_OOP_RASTERIZATION] =
        kGpuFeatureStatusBlocklisted;
  }
  // If disable_webnn_for_gpu workaround is enabled for the GPU device, we need
  // to check to see if there is a NPU device available before setting the WebNN
  // gpu feature status. If there is a NPU device, check the
  // disable_webnn_for_npu workaround.
  if (gpu_feature_info->IsWorkaroundEnabled(DISABLE_WEBNN_FOR_GPU)) {
    if (gpu_info.npus.size() > 0) {
      if (gpu_feature_info->IsWorkaroundEnabled(DISABLE_WEBNN_FOR_NPU)) {
        gpu_feature_info->status_values[GPU_FEATURE_TYPE_WEBNN] =
            kGpuFeatureStatusSoftware;
      }
    } else {
      gpu_feature_info->status_values[GPU_FEATURE_TYPE_WEBNN] =
          kGpuFeatureStatusSoftware;
    }
  }
}

// Estimates roughly user total disk space by counting in the drives where
// the exe is, where the temporary space is, where the user home is.
// If total space and free space are of the same size, they are considered
// the same drive. There could be corner cases this estimation is far from
// the actual total disk space, but for histogram purpose, limited numbers
// of outliers do not matter.
uint32_t EstimateAmountOfTotalDiskSpaceMB() {
  const base::BasePathKey kPathKeys[] = {base::DIR_EXE, base::DIR_TEMP,
                                         base::DIR_HOME};
  std::vector<uint32_t> total_space_vector, free_space_vector;
  uint32_t sum = 0;
  for (const auto& path_key : kPathKeys) {
    base::FilePath path;
    if (base::PathService::Get(path_key, &path)) {
      uint32_t total_space = static_cast<uint32_t>(
          base::SysInfo::AmountOfTotalDiskSpace(path) / 1024 / 1024);
      uint32_t free_space = static_cast<uint32_t>(
          base::SysInfo::AmountOfFreeDiskSpace(path) / 1024 / 1024);
      bool duplicated = false;
      for (size_t ii = 0; ii < total_space_vector.size(); ++ii) {
        if (total_space == total_space_vector[ii] &&
            free_space == free_space_vector[ii]) {
          duplicated = true;
          break;
        }
      }
      if (!duplicated) {
        total_space_vector.push_back(total_space);
        free_space_vector.push_back(free_space);
        sum += total_space;
      }
    }
  }
  return sum;
}

// Only record Nvidia and AMD GPUs.
void RecordGpuHistogram(uint32_t vendor_id, uint32_t device_id) {
  switch (vendor_id) {
    case 0x10de:
      base::SparseHistogram::FactoryGet(
          "GPU.MultiGpu.Nvidia", base::HistogramBase::kUmaTargetedHistogramFlag)
          ->Add(device_id);
      break;
    case 0x1002:
      base::SparseHistogram::FactoryGet(
          "GPU.MultiGpu.AMD", base::HistogramBase::kUmaTargetedHistogramFlag)
          ->Add(device_id);
      break;
    default:
      // Do nothing if it's not Nvidia/AMD.
      break;
  }
}

#if BUILDFLAG(IS_WIN)
uint32_t GetSystemCommitLimitMb() {
  PERFORMANCE_INFORMATION perf_info = {sizeof(perf_info)};
  if (::GetPerformanceInfo(&perf_info, sizeof(perf_info))) {
    uint64_t limit = perf_info.CommitLimit;
    limit *= perf_info.PageSize;
    limit /= 1024 * 1024;
    return static_cast<uint32_t>(limit);
  }
  return 0u;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
GPUInfo* g_gpu_info_cache = nullptr;
GpuFeatureInfo* g_gpu_feature_info_cache = nullptr;
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

GpuFeatureInfo ComputeGpuFeatureInfoWithNoGpu() {
  GpuFeatureInfo gpu_feature_info;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS] =
      kGpuFeatureStatusSoftware;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL2] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_GL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_VULKAN] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_CANVAS_OOP_RASTERIZATION] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGPU] =
      kGpuFeatureStatusSoftware;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_SKIA_GRAPHITE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_WEBNN] =
      kGpuFeatureStatusSoftware;
#if DCHECK_IS_ON()
  for (int ii = 0; ii < NUMBER_OF_GPU_FEATURE_TYPES; ++ii) {
    DCHECK_NE(kGpuFeatureStatusUndefined, gpu_feature_info.status_values[ii]);
  }
#endif
  return gpu_feature_info;
}

GpuFeatureInfo ComputeGpuFeatureInfoForSwiftShader() {
  GpuFeatureInfo gpu_feature_info;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS] =
      kGpuFeatureStatusSoftware;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL] =
      kGpuFeatureStatusSoftware;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL2] =
      kGpuFeatureStatusSoftware;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_GL] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_VULKAN] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_CANVAS_OOP_RASTERIZATION] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGPU] =
      kGpuFeatureStatusSoftware;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_SKIA_GRAPHITE] =
      kGpuFeatureStatusDisabled;
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_WEBNN] =
      kGpuFeatureStatusSoftware;
#if DCHECK_IS_ON()
  for (int ii = 0; ii < NUMBER_OF_GPU_FEATURE_TYPES; ++ii) {
    DCHECK_NE(kGpuFeatureStatusUndefined, gpu_feature_info.status_values[ii]);
  }
#endif
  return gpu_feature_info;
}

GpuFeatureInfo ComputeGpuFeatureInfo(const GPUInfo& gpu_info,
                                     const GpuPreferences& gpu_preferences,
                                     base::CommandLine* command_line,
                                     bool* needs_more_info) {
  bool use_swift_shader = false;
  bool blocklist_needs_more_info = false;

  std::optional<gl::GLImplementationParts> requested_impl =
      gl::GetRequestedGLImplementationFromCommandLine(command_line);
  if (requested_impl) {
    if (*requested_impl == gl::kGLImplementationNone)
      return ComputeGpuFeatureInfoWithNoGpu();

    use_swift_shader = gl::IsSoftwareGLImplementation(*requested_impl);
    if (use_swift_shader) {
      std::string use_gl = command_line->GetSwitchValueASCII(switches::kUseGL);
      std::string use_angle =
          command_line->GetSwitchValueASCII(switches::kUseANGLE);
      if (use_angle == gl::kANGLEImplementationSwiftShaderForWebGLName) {
        return ComputeGpuFeatureInfoForSwiftShader();
      }
    }
  }

  if (gpu_preferences.use_vulkan ==
      gpu::VulkanImplementationName::kSwiftshader) {
    use_swift_shader = true;
  }

  GpuFeatureInfo gpu_feature_info;
  std::set<int> blocklisted_features;
  if (!gpu_preferences.ignore_gpu_blocklist &&
      !command_line->HasSwitch(switches::kUseGpuInTests)) {
    std::unique_ptr<GpuBlocklist> list(GpuBlocklist::Create());
    if (gpu_preferences.log_gpu_control_list_decisions)
      list->EnableControlListLogging("gpu_blocklist");
    unsigned target_test_group = 0u;
    if (command_line->HasSwitch(switches::kGpuBlocklistTestGroup)) {
      std::string test_group_string =
          command_line->GetSwitchValueASCII(switches::kGpuBlocklistTestGroup);
      if (!base::StringToUint(test_group_string, &target_test_group))
        target_test_group = 0u;
    }
    blocklisted_features = list->MakeDecision(
        GpuControlList::kOsAny, std::string(), gpu_info, target_test_group);
    gpu_feature_info.applied_gpu_blocklist_entries = list->GetActiveEntries();
    blocklist_needs_more_info = list->needs_more_info();
  }

  if (needs_more_info) {
    *needs_more_info = blocklist_needs_more_info;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] =
      GetGpuRasterizationFeatureStatus(blocklisted_features, *command_line,
                                       use_swift_shader);
#else
  // TODO(penghuang): call GetGpuRasterizationFeatureStatus() with
  // |use_swift_shader|.
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] =
      GetGpuRasterizationFeatureStatus(blocklisted_features, *command_line,
                                       /*use_swift_shader=*/false);
#endif
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL] =
      GetWebGLFeatureStatus(blocklisted_features, use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL2] =
      GetWebGL2FeatureStatus(blocklisted_features, use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGPU] =
      GetWebGPUFeatureStatus(blocklisted_features, use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS] =
      Get2DCanvasFeatureStatus(blocklisted_features, use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_CANVAS_OOP_RASTERIZATION] =
      GetCanvasOopRasterizationFeatureStatus(blocklisted_features,
                                             gpu_preferences);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] =
      GetAcceleratedVideoDecodeFeatureStatus(blocklisted_features,
                                             use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE] =
      GetAcceleratedVideoEncodeFeatureStatus(blocklisted_features,
                                             use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] =
      GetAndroidSurfaceControlFeatureStatus(blocklisted_features,
                                            gpu_preferences);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_GL] =
      GetGLFeatureStatus(blocklisted_features, use_swift_shader);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_VULKAN] =
      GetVulkanFeatureStatus(blocklisted_features, gpu_preferences);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_SKIA_GRAPHITE] =
      GetSkiaGraphiteFeatureStatus(blocklisted_features, gpu_preferences);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_WEBNN] =
      GetWebNNFeatureStatus(blocklisted_features);
#if DCHECK_IS_ON()
  for (int ii = 0; ii < NUMBER_OF_GPU_FEATURE_TYPES; ++ii) {
    DCHECK_NE(kGpuFeatureStatusUndefined, gpu_feature_info.status_values[ii]);
  }
#endif

  gfx::ExtensionSet all_disabled_extensions;
  std::string disabled_gl_extensions_value =
      command_line->GetSwitchValueASCII(switches::kDisableGLExtensions);
  if (!disabled_gl_extensions_value.empty()) {
    std::vector<std::string_view> command_line_disabled_extensions =
        base::SplitStringPiece(disabled_gl_extensions_value, ", ;",
                               base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    all_disabled_extensions.insert(command_line_disabled_extensions.begin(),
                                   command_line_disabled_extensions.end());
  }

  std::set<int> enabled_driver_bug_workarounds;
  std::vector<std::string> driver_bug_disabled_extensions;
  if (!command_line->HasSwitch(switches::kDisableGpuDriverBugWorkarounds)) {
    std::unique_ptr<gpu::GpuDriverBugList> list(GpuDriverBugList::Create());
    unsigned target_test_group = 0u;
    if (command_line->HasSwitch(switches::kGpuDriverBugListTestGroup)) {
      std::string test_group_string = command_line->GetSwitchValueASCII(
          switches::kGpuDriverBugListTestGroup);
      if (!base::StringToUint(test_group_string, &target_test_group))
        target_test_group = 0u;
    }
    enabled_driver_bug_workarounds = list->MakeDecision(
        GpuControlList::kOsAny, std::string(), gpu_info, target_test_group);
    gpu_feature_info.applied_gpu_driver_bug_list_entries =
        list->GetActiveEntries();

    driver_bug_disabled_extensions = list->GetDisabledExtensions();
    all_disabled_extensions.insert(driver_bug_disabled_extensions.begin(),
                                   driver_bug_disabled_extensions.end());

    // Disabling WebGL extensions only occurs via the blocklist, so
    // the logic is simpler.
    gfx::ExtensionSet disabled_webgl_extensions;
    std::vector<std::string> disabled_webgl_extension_list =
        list->GetDisabledWebGLExtensions();
    disabled_webgl_extensions.insert(disabled_webgl_extension_list.begin(),
                                     disabled_webgl_extension_list.end());
    gpu_feature_info.disabled_webgl_extensions =
        gfx::MakeExtensionString(disabled_webgl_extensions);
  }
  gpu::GpuDriverBugList::AppendWorkaroundsFromCommandLine(
      &enabled_driver_bug_workarounds, *command_line);

  gpu_feature_info.enabled_gpu_driver_bug_workarounds.insert(
      gpu_feature_info.enabled_gpu_driver_bug_workarounds.begin(),
      enabled_driver_bug_workarounds.begin(),
      enabled_driver_bug_workarounds.end());

  if (all_disabled_extensions.size()) {
    gpu_feature_info.disabled_extensions =
        gfx::MakeExtensionString(all_disabled_extensions);
  }

  AdjustGpuFeatureStatusToWorkarounds(&gpu_feature_info, gpu_info);

  SetProcessGlWorkaroundsFromGpuFeatures(gpu_feature_info);

  return gpu_feature_info;
}

void SetKeysForCrashLogging(const GPUInfo& gpu_info) {
  const GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();
#if !BUILDFLAG(IS_ANDROID)
  crash_keys::gpu_vendor_id.Set(
      base::StringPrintf("0x%04x", active_gpu.vendor_id));
  crash_keys::gpu_device_id.Set(
      base::StringPrintf("0x%04x", active_gpu.device_id));
  crash_keys::gpu_count.Set(base::StringPrintf("%d", gpu_info.GpuCount()));
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN)
  crash_keys::gpu_sub_sys_id.Set(
      base::StringPrintf("0x%08x", active_gpu.sub_sys_id));
  crash_keys::gpu_revision.Set(base::StringPrintf("%u", active_gpu.revision));
#endif  // BUILDFLAG(IS_WIN)
  crash_keys::gpu_driver_version.Set(active_gpu.driver_version);
  crash_keys::gpu_pixel_shader_version.Set(gpu_info.pixel_shader_version);
  crash_keys::gpu_vertex_shader_version.Set(gpu_info.vertex_shader_version);
  crash_keys::gpu_generation_intel.Set(base::StringPrintf(
      "%d", static_cast<int>(GetIntelGpuGeneration(gpu_info))));
#if BUILDFLAG(IS_MAC)
  crash_keys::gpu_gl_version.Set(gpu_info.gl_version);
#elif BUILDFLAG(IS_POSIX)
  crash_keys::gpu_vendor.Set(gpu_info.gl_vendor);
  crash_keys::gpu_renderer.Set(gpu_info.gl_renderer);
#endif
}

#if BUILDFLAG(IS_ANDROID)
void CacheGPUInfo(const GPUInfo& gpu_info) {
  DCHECK(!g_gpu_info_cache);
  g_gpu_info_cache = new GPUInfo;
  *g_gpu_info_cache = gpu_info;
}

bool PopGPUInfoCache(GPUInfo* gpu_info) {
  if (!g_gpu_info_cache)
    return false;
  *gpu_info = *g_gpu_info_cache;
  delete g_gpu_info_cache;
  g_gpu_info_cache = nullptr;
  return true;
}

void CacheGpuFeatureInfo(const GpuFeatureInfo& gpu_feature_info) {
  DCHECK(!g_gpu_feature_info_cache);
  g_gpu_feature_info_cache = new GpuFeatureInfo;
  *g_gpu_feature_info_cache = gpu_feature_info;
}

bool PopGpuFeatureInfoCache(GpuFeatureInfo* gpu_feature_info) {
  if (!g_gpu_feature_info_cache)
    return false;
  *gpu_feature_info = *g_gpu_feature_info_cache;
  delete g_gpu_feature_info_cache;
  g_gpu_feature_info_cache = nullptr;
  return true;
}

gl::GLDisplay* InitializeGLThreadSafe(base::CommandLine* command_line,
                                      const GpuPreferences& gpu_preferences,
                                      GPUInfo* out_gpu_info,
                                      GpuFeatureInfo* out_gpu_feature_info) {
  static base::NoDestructor<base::Lock> gl_bindings_initialization_lock;
  base::AutoLock auto_lock(*gl_bindings_initialization_lock);
  DCHECK(command_line);
  DCHECK(out_gpu_info && out_gpu_feature_info);
  bool gpu_info_cached = PopGPUInfoCache(out_gpu_info);
  bool gpu_feature_info_cached = PopGpuFeatureInfoCache(out_gpu_feature_info);
  DCHECK_EQ(gpu_info_cached, gpu_feature_info_cached);
  if (gpu_info_cached) {
    // GL bindings have already been initialized in another thread.
    DCHECK_NE(gl::kGLImplementationNone, gl::GetGLImplementation());
    return gl::GetDefaultDisplayEGL();
  }

  gl::GLDisplay* gl_display = nullptr;
  if (gl::GetGLImplementation() == gl::kGLImplementationNone) {
    // Some tests initialize bindings by themselves.
    gl_display = gl::init::InitializeGLNoExtensionsOneOff(
        /*init_bindings=*/true,
        /*gpu_preference=*/gl::GpuPreference::kDefault);
    if (!gl_display) {
      VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed";
      return nullptr;
    }
  } else {
    gl_display = gl::GetDefaultDisplayEGL();
  }
  CollectContextGraphicsInfo(out_gpu_info);
  *out_gpu_feature_info = ComputeGpuFeatureInfo(*out_gpu_info, gpu_preferences,
                                                command_line, nullptr);
  if (!out_gpu_feature_info->disabled_extensions.empty()) {
    gl::init::SetDisabledExtensionsPlatform(
        out_gpu_feature_info->disabled_extensions);
  }
  if (!gl::init::InitializeExtensionSettingsOneOffPlatform(gl_display)) {
    VLOG(1) << "gl::init::InitializeExtensionSettingsOneOffPlatform failed";
    return nullptr;
  }
  CacheGPUInfo(*out_gpu_info);
  CacheGpuFeatureInfo(*out_gpu_feature_info);
  return gl_display;
}
#endif  // BUILDFLAG(IS_ANDROID)

bool EnableSwiftShaderIfNeeded(base::CommandLine* command_line,
                               const GpuFeatureInfo& gpu_feature_info,
                               bool disable_software_rasterizer,
                               bool blocklist_needs_more_info) {
#if BUILDFLAG(ENABLE_SWIFTSHADER)
  if (disable_software_rasterizer || blocklist_needs_more_info)
    return false;
  // Don't overwrite user preference.
  if (command_line->HasSwitch(switches::kUseGL))
    return false;
  if (gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL] !=
          kGpuFeatureStatusEnabled ||
      gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_GL] !=
          kGpuFeatureStatusEnabled) {
    gl::SetSoftwareWebGLCommandLineSwitches(command_line);
    return true;
  }
  return false;
#else
  return false;
#endif
}

IntelGpuSeriesType GetIntelGpuSeriesType(uint32_t vendor_id,
                                         uint32_t device_id) {
  // Note that this function's output should only depend on vendor_id and
  // device_id of a GPU. This is because we record a histogram on the output
  // and we don't want to expose an extra bit other than the already recorded
  // vendor_id and device_id.
  if (vendor_id == 0x8086) {  // Intel
    // The device IDs of Intel GPU are based on following Mesa source files:
    // include/pci_ids/i965_pci_ids.h and iris_pci_ids.h
    uint32_t masked_device_id = device_id & 0xFF00;
    switch (masked_device_id) {
      case 0x2900:
        return IntelGpuSeriesType::kBroadwater;
      case 0x2A00:
        if (device_id == 0x2A02 || device_id == 0x2A12)
          return IntelGpuSeriesType::kBroadwater;
        if (device_id == 0x2A42)
          return IntelGpuSeriesType::kEaglelake;
        break;
      case 0x2E00:
        return IntelGpuSeriesType::kEaglelake;
      case 0x0000:
        return IntelGpuSeriesType::kIronlake;
      case 0x0100:
        if (device_id == 0x0152 || device_id == 0x0156 || device_id == 0x015A ||
            device_id == 0x0162 || device_id == 0x0166 || device_id == 0x016A)
          return IntelGpuSeriesType::kIvybridge;
        if (device_id == 0x0155 || device_id == 0x0157)
          return IntelGpuSeriesType::kBaytrail;
        return IntelGpuSeriesType::kSandybridge;
      case 0x0F00:
        return IntelGpuSeriesType::kBaytrail;
      case 0x0A00:
        if (device_id == 0x0A84)
          return IntelGpuSeriesType::kApollolake;
        return IntelGpuSeriesType::kHaswell;
      case 0x0400:
      case 0x0C00:
      case 0x0D00:
        return IntelGpuSeriesType::kHaswell;
      case 0x2200:
        return IntelGpuSeriesType::kCherrytrail;
      case 0x1600:
        return IntelGpuSeriesType::kBroadwell;
      case 0x5A00:
        if (device_id == 0x5A85 || device_id == 0x5A84)
          return IntelGpuSeriesType::kApollolake;
        return IntelGpuSeriesType::kCannonlake;
      case 0x1900:
        return IntelGpuSeriesType::kSkylake;
      case 0x1A00:
        return IntelGpuSeriesType::kApollolake;
      case 0x3100:
        return IntelGpuSeriesType::kGeminilake;
      case 0x5900:
        if (device_id == 0x591C)
          return IntelGpuSeriesType::kAmberlake;
        return IntelGpuSeriesType::kKabylake;
      case 0x8700:
        if (device_id == 0x87C0)
          return IntelGpuSeriesType::kKabylake;
        if (device_id == 0x87CA)
          return IntelGpuSeriesType::kCoffeelake;
        break;
      case 0x3E00:
        if (device_id == 0x3EA0 || device_id == 0x3EA1 || device_id == 0x3EA2
            || device_id == 0x3EA4 || device_id == 0x3EA3)
          return IntelGpuSeriesType::kWhiskeylake;
        return IntelGpuSeriesType::kCoffeelake;
      case 0x9B00:
        return IntelGpuSeriesType::kCometlake;
      case 0x8A00:
        return IntelGpuSeriesType::kIcelake;
      case 0x4500:
        return IntelGpuSeriesType::kElkhartlake;
      case 0x4E00:
        return IntelGpuSeriesType::kJasperlake;
      case 0x9A00:
        return IntelGpuSeriesType::kTigerlake;
      case 0x4c00:
        return IntelGpuSeriesType::kRocketlake;
      case 0x4900:
        return IntelGpuSeriesType::kDG1;
      case 0x4600:
        return IntelGpuSeriesType::kAlderlake;
      case 0x4F00:
      case 0x5600:
        return IntelGpuSeriesType::kAlchemist;
      case 0xA700:
        return IntelGpuSeriesType::kRaptorlake;
      case 0x7D00:
        if (device_id == 0x7D41 || device_id == 0x7D51 || device_id == 0x7D67 ||
            device_id == 0x7DD1) {
          return IntelGpuSeriesType::kArrowlake;
        }
        return IntelGpuSeriesType::kMeteorlake;
      case 0x6400:
        return IntelGpuSeriesType::kLunarlake;
      case 0xE200:
        return IntelGpuSeriesType::kBattlemage;
      default:
        break;
    }
  }
  return IntelGpuSeriesType::kUnknown;
}

std::string GetIntelGpuGeneration(uint32_t vendor_id, uint32_t device_id) {
  if (vendor_id == 0x8086) {
    IntelGpuSeriesType gpu_series = GetIntelGpuSeriesType(vendor_id, device_id);
    switch (gpu_series) {
      case IntelGpuSeriesType::kBroadwater:
      case IntelGpuSeriesType::kEaglelake:
        return "4";
      case IntelGpuSeriesType::kIronlake:
        return "5";
      case IntelGpuSeriesType::kSandybridge:
        return "6";
      case IntelGpuSeriesType::kBaytrail:
      case IntelGpuSeriesType::kIvybridge:
      case IntelGpuSeriesType::kHaswell:
        return "7";
      case IntelGpuSeriesType::kCherrytrail:
      case IntelGpuSeriesType::kBroadwell:
        return "8";
      case IntelGpuSeriesType::kApollolake:
      case IntelGpuSeriesType::kSkylake:
      case IntelGpuSeriesType::kGeminilake:
      case IntelGpuSeriesType::kKabylake:
      case IntelGpuSeriesType::kAmberlake:
      case IntelGpuSeriesType::kCoffeelake:
      case IntelGpuSeriesType::kWhiskeylake:
      case IntelGpuSeriesType::kCometlake:
        return "9";
      case IntelGpuSeriesType::kCannonlake:
        return "10";
      case IntelGpuSeriesType::kIcelake:
      case IntelGpuSeriesType::kElkhartlake:
      case IntelGpuSeriesType::kJasperlake:
        return "11";
      case IntelGpuSeriesType::kTigerlake:
      case IntelGpuSeriesType::kRocketlake:
      case IntelGpuSeriesType::kDG1:
      case IntelGpuSeriesType::kAlderlake:
      case IntelGpuSeriesType::kAlchemist:
      case IntelGpuSeriesType::kRaptorlake:
      case IntelGpuSeriesType::kMeteorlake:
      case IntelGpuSeriesType::kArrowlake:
        return "12";
      case IntelGpuSeriesType::kLunarlake:
      case IntelGpuSeriesType::kBattlemage:
        return "13";
      default:
        break;
    }
  }
  return "";
}

IntelGpuGeneration GetIntelGpuGeneration(const GPUInfo& gpu_info) {
  const uint32_t kIntelVendorId = 0x8086;
  IntelGpuGeneration latest = IntelGpuGeneration::kNonIntel;
  std::vector<uint32_t> intel_device_ids;
  if (gpu_info.gpu.vendor_id == kIntelVendorId)
    intel_device_ids.push_back(gpu_info.gpu.device_id);
  for (const auto& gpu : gpu_info.secondary_gpus) {
    if (gpu.vendor_id == kIntelVendorId)
      intel_device_ids.push_back(gpu.device_id);
  }
  if (intel_device_ids.empty())
    return latest;
  latest = IntelGpuGeneration::kUnknownIntel;
  for (uint32_t device_id : intel_device_ids) {
    std::string gen_str = gpu::GetIntelGpuGeneration(kIntelVendorId, device_id);
    int gen_int = 0;
    if (gen_str.empty() || !base::StringToInt(gen_str, &gen_int))
      continue;
    DCHECK_GE(gen_int, static_cast<int>(IntelGpuGeneration::kUnknownIntel));
    DCHECK_LE(gen_int, static_cast<int>(IntelGpuGeneration::kMaxValue));
    if (gen_int > static_cast<int>(latest))
      latest = static_cast<IntelGpuGeneration>(gen_int);
  }
  return latest;
}

void CollectDevicePerfInfo(DevicePerfInfo* device_perf_info,
                           bool in_browser_process) {
  DCHECK(device_perf_info);
  device_perf_info->total_physical_memory_mb =
      static_cast<uint32_t>(base::SysInfo::AmountOfPhysicalMemoryMB());
  if (!in_browser_process)
    device_perf_info->total_disk_space_mb = EstimateAmountOfTotalDiskSpaceMB();
  device_perf_info->hardware_concurrency =
      static_cast<uint32_t>(std::thread::hardware_concurrency());

#if BUILDFLAG(IS_WIN)
  device_perf_info->system_commit_limit_mb = GetSystemCommitLimitMb();
  if (!in_browser_process) {
    D3D_FEATURE_LEVEL d3d11_feature_level = D3D_FEATURE_LEVEL_1_0_CORE;
    bool has_discrete_gpu = false;
    if (CollectD3D11FeatureInfo(&d3d11_feature_level, &has_discrete_gpu)) {
      device_perf_info->d3d11_feature_level = d3d11_feature_level;
      device_perf_info->has_discrete_gpu =
          has_discrete_gpu ? HasDiscreteGpu::kYes : HasDiscreteGpu::kNo;
    }
  }
#endif
}

void RecordDevicePerfInfoHistograms() {
  std::optional<DevicePerfInfo> device_perf_info = GetDevicePerfInfo();
  if (!device_perf_info.has_value())
    return;
  UMA_HISTOGRAM_COUNTS_1000("Hardware.TotalDiskSpace",
                            device_perf_info->total_disk_space_mb / 1024);
#if BUILDFLAG(IS_WIN)
  UMA_HISTOGRAM_COUNTS_100("Memory.Total.SystemCommitLimit",
                           device_perf_info->system_commit_limit_mb / 1024);
  UMA_HISTOGRAM_ENUMERATION("GPU.D3D11FeatureLevel",
                            ConvertToHistogramD3D11FeatureLevel(
                                device_perf_info->d3d11_feature_level));
  UMA_HISTOGRAM_ENUMERATION("GPU.HasDiscreteGpu",
                            device_perf_info->has_discrete_gpu);
#endif  // BUILDFLAG(IS_WIN)
  UMA_HISTOGRAM_ENUMERATION("GPU.IntelGpuGeneration",
                            device_perf_info->intel_gpu_generation);
  UMA_HISTOGRAM_BOOLEAN("GPU.SoftwareRendering",
                        device_perf_info->software_rendering);
}

void RecordDiscreteGpuHistograms(const GPUInfo& gpu_info) {
  if (gpu_info.GpuCount() < 2)
    return;
  // To simplify logic, if there are multiple GPUs identified on a device,
  // assume AMD or Nvidia is the discrete GPU.
  RecordGpuHistogram(gpu_info.gpu.vendor_id, gpu_info.gpu.device_id);
  for (const auto& gpu : gpu_info.secondary_gpus)
    RecordGpuHistogram(gpu.vendor_id, gpu.device_id);
}

#if BUILDFLAG(IS_WIN)
std::string DirectMLFeatureLevelToString(uint32_t directml_feature_level) {
  if (directml_feature_level == 0) {
    return "Not supported";
  } else {
    return base::StringPrintf("%d.%d", (directml_feature_level >> 12) & 0xF,
                              (directml_feature_level >> 8) & 0xF);
  }
}

std::string D3DFeatureLevelToString(uint32_t d3d_feature_level) {
  if (d3d_feature_level == 0) {
    return "Not supported";
  } else {
    return base::StringPrintf("D3D %d.%d", (d3d_feature_level >> 12) & 0xF,
                              (d3d_feature_level >> 8) & 0xF);
  }
}

std::string VulkanVersionToString(uint32_t vulkan_version) {
  if (vulkan_version == 0) {
    return "Not supported";
  } else {
    // Vulkan version number VK_MAKE_VERSION(major, minor, patch)
    // (((major) << 22) | ((minor) << 12) | (patch))
    return base::StringPrintf(
        "Vulkan API %d.%d.%d", (vulkan_version >> 22) & 0x3FF,
        (vulkan_version >> 12) & 0x3FF, vulkan_version & 0xFFF);
  }
}
#endif  // BUILDFLAG(IS_WIN)
}  // namespace gpu

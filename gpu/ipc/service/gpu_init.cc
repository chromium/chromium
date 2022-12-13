// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_init.h"

#include <cstdlib>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_switching.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/switches.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_MAC)
#include <GLES2/gl2.h>
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_surface_egl.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_image_reader_compat.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_util.h"
#endif

#if defined(USE_EGL) && !BUILDFLAG(IS_MAC)
#include "ui/gl/gl_fence_egl.h"
#endif

namespace gpu {

namespace {
bool CollectGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);
  TRACE_EVENT0("gpu,startup", "Collect Graphics Info");
  bool success = CollectContextGraphicsInfo(gpu_info);
  if (!success)
    LOG(ERROR) << "CollectGraphicsInfo failed.";
  return success;
}

void InitializePlatformOverlaySettings(GPUInfo* gpu_info,
                                       const GpuFeatureInfo& gpu_feature_info) {
#if BUILDFLAG(IS_WIN)
  // This has to be called after a context is created, active GPU is identified,
  // and GPU driver bug workarounds are computed again. Otherwise the workaround
  // |disable_direct_composition| may not be correctly applied.
  // Also, this has to be called after falling back to SwiftShader decision is
  // finalized because this function depends on GL is ANGLE's GLES or not.
  gl::DirectCompositionOverlayWorkarounds workarounds = {
      .enable_bgra8_overlays_with_yuv_overlay_support =
          gpu_feature_info.IsWorkaroundEnabled(
              gpu::ENABLE_BGRA8_OVERLAYS_WITH_YUV_OVERLAY_SUPPORT),
      .force_nv12_overlay_support =
          gpu_feature_info.IsWorkaroundEnabled(gpu::FORCE_NV12_OVERLAY_SUPPORT),
      .force_rgb10a2_overlay_support = gpu_feature_info.IsWorkaroundEnabled(
          gpu::FORCE_RGB10A2_OVERLAY_SUPPORT_FLAGS),
      .check_ycbcr_studio_g22_left_p709_for_nv12_support =
          gpu_feature_info.IsWorkaroundEnabled(
              gpu::CHECK_YCBCR_STUDIO_G22_LEFT_P709_FOR_NV12_SUPPORT)};
  SetDirectCompositionOverlayWorkarounds(workarounds);

  DCHECK(gpu_info);
  CollectHardwareOverlayInfo(&gpu_info->overlay_info);
#elif BUILDFLAG(IS_ANDROID)
  if (gpu_info->gpu.vendor_string == "Qualcomm")
    gfx::SurfaceControl::EnableQualcommUBWC();
#endif
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CASTOS)
bool CanAccessDeviceFile(const GPUInfo& gpu_info) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (gpu_info.gpu.vendor_id != 0x10de ||  // NVIDIA
      gpu_info.gpu.driver_vendor != "NVIDIA")
    return true;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  if (access("/dev/nvidiactl", R_OK) != 0) {
    DVLOG(1) << "NVIDIA device file /dev/nvidiactl access denied";
    return false;
  }
  return true;
#else
  return true;
#endif
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CASTOS)

class GpuWatchdogInit {
 public:
  GpuWatchdogInit() = default;
  ~GpuWatchdogInit() {
    if (watchdog_ptr_)
      watchdog_ptr_->OnInitComplete();
  }

  void SetGpuWatchdogPtr(GpuWatchdogThread* ptr) { watchdog_ptr_ = ptr; }

 private:
  GpuWatchdogThread* watchdog_ptr_ = nullptr;
};

// TODO(https://crbug.com/1095744): We currently do not handle
// VK_ERROR_DEVICE_LOST in in-process-gpu.
// Android WebView is allowed for now because it CHECKs on context loss.
void DisableInProcessGpuVulkan(GpuFeatureInfo* gpu_feature_info,
                               GpuPreferences* gpu_preferences) {
  if (gpu_feature_info->status_values[GPU_FEATURE_TYPE_VULKAN] ==
          kGpuFeatureStatusEnabled ||
      gpu_preferences->gr_context_type == GrContextType::kVulkan) {
    LOG(ERROR) << "Vulkan not supported with in process gpu";
    gpu_preferences->use_vulkan = VulkanImplementationName::kNone;
    gpu_feature_info->status_values[GPU_FEATURE_TYPE_VULKAN] =
        kGpuFeatureStatusDisabled;
    gpu_preferences->gr_context_type = GrContextType::kGL;
  }
}

#if BUILDFLAG(ENABLE_VULKAN)
bool MatchGLInfo(const std::string& field, const std::string& patterns) {
  auto pattern_strings = base::SplitString(patterns, "|", base::TRIM_WHITESPACE,
                                           base::SPLIT_WANT_ALL);
  for (const auto& pattern : pattern_strings) {
    if (base::MatchPattern(field, pattern))
      return true;
  }
  return false;
}
#endif  // BUILDFLAG(ENABLE_VULKAN)

#if BUILDFLAG(IS_WIN)
uint64_t CHROME_LUID_to_uint64_t(const CHROME_LUID& luid) {
  uint64_t id64 = static_cast<uint32_t>(luid.HighPart);
  return (id64 << 32) | (luid.LowPart & 0xFFFFFFFF);
}
#endif  // BUILDFLAG(IS_WIN)

#if defined(USE_EGL) && (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC))
// GPU picking is only effective with ANGLE/Metal backend on Mac and
// on Windows with EGL.
// Returns the default GPU's system_device_id.
uint64_t SetupGLDisplayManagerEGL(const GPUInfo& gpu_info,
                                  const GpuFeatureInfo& gpu_feature_info) {
  const GPUInfo::GPUDevice* gpu_high_perf =
      gpu_info.GetGpuByPreference(gl::GpuPreference::kHighPerformance);
  const GPUInfo::GPUDevice* gpu_low_power =
      gpu_info.GetGpuByPreference(gl::GpuPreference::kLowPower);
#if BUILDFLAG(IS_WIN)
  // On Windows the default GPU may not be the low power GPU.
  const GPUInfo::GPUDevice* gpu_default = &(gpu_info.gpu);
  uint64_t system_device_id_high_perf =
      gpu_high_perf ? CHROME_LUID_to_uint64_t(gpu_high_perf->luid) : 0;
  uint64_t system_device_id_low_power =
      gpu_low_power ? CHROME_LUID_to_uint64_t(gpu_low_power->luid) : 0;
  uint64_t system_device_id_default =
      CHROME_LUID_to_uint64_t(gpu_default->luid);
#else  // IS_MAC
  const GPUInfo::GPUDevice* gpu_default =
      gpu_low_power ? gpu_low_power : &(gpu_info.gpu);
  uint64_t system_device_id_high_perf =
      gpu_high_perf ? gpu_high_perf->register_id : 0;
  uint64_t system_device_id_low_power =
      gpu_low_power ? gpu_low_power->register_id : 0;
  uint64_t system_device_id_default = gpu_default->register_id;
#endif
  DCHECK(gpu_default);

  if (gpu_info.GpuCount() <= 1) {
    gl::SetGpuPreferenceEGL(gl::GpuPreference::kDefault,
                            system_device_id_default);
    return system_device_id_default;
  }
  if (gpu_feature_info.IsWorkaroundEnabled(FORCE_LOW_POWER_GPU) &&
      system_device_id_low_power) {
    gl::SetGpuPreferenceEGL(gl::GpuPreference::kDefault,
                            system_device_id_low_power);
    return system_device_id_low_power;
  }
  if (gpu_feature_info.IsWorkaroundEnabled(FORCE_HIGH_PERFORMANCE_GPU) &&
      system_device_id_high_perf) {
    gl::SetGpuPreferenceEGL(gl::GpuPreference::kDefault,
                            system_device_id_high_perf);
    return system_device_id_high_perf;
  }
  if (gpu_default == gpu_high_perf) {
    // If the default GPU is already the high performance GPU, then it's better
    // for Chrome to always use this GPU.
    gl::SetGpuPreferenceEGL(gl::GpuPreference::kDefault,
                            system_device_id_high_perf);
    return system_device_id_high_perf;
  }

  // Chrome uses the default GPU for internal rendering and the high
  // performance GPU for WebGL/WebGPU contexts that prefer high performance.
  // At this moment, a low power GPU different from the default GPU is not
  // supported.
  gl::SetGpuPreferenceEGL(gl::GpuPreference::kDefault,
                          system_device_id_default);
  if (system_device_id_high_perf && features::SupportsEGLDualGPURendering()) {
    gl::SetGpuPreferenceEGL(gl::GpuPreference::kHighPerformance,
                            system_device_id_high_perf);
  }
  return system_device_id_default;
}
#endif  // USE_EGL && (IS_WIN || IS_MAC)

}  // namespace

GpuInit::GpuInit() = default;

GpuInit::~GpuInit() {
  StopForceDiscreteGPU();
}

bool GpuInit::InitializeAndStartSandbox(base::CommandLine* command_line,
                                        const GpuPreferences& gpu_preferences) {
  gpu_preferences_ = gpu_preferences;
  // Blocklist decisions based on basic GPUInfo may not be final. It might
  // need more context based GPUInfo. In such situations, switching to
  // SwiftShader needs to wait until creating a context.
  bool needs_more_info = true;
  uint64_t system_device_id = 0;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CASTOS)
  needs_more_info = false;
  if (!PopGPUInfoCache(&gpu_info_)) {
    CollectBasicGraphicsInfo(command_line, &gpu_info_);
  }

  IntelGpuSeriesType intel_gpu_series_type = GetIntelGpuSeriesType(
      gpu_info_.active_gpu().vendor_id, gpu_info_.active_gpu().device_id);
  UMA_HISTOGRAM_ENUMERATION("GPU.IntelGpuSeriesType", intel_gpu_series_type);

  // Set keys for crash logging based on preliminary gpu info, in case we
  // crash during feature collection.
  SetKeysForCrashLogging(gpu_info_);
#if defined(SUBPIXEL_FONT_RENDERING_DISABLED)
  gpu_info_.subpixel_font_rendering = false;
#else
  gpu_info_.subpixel_font_rendering = true;
#endif

  if (gpu_preferences_.enable_perf_data_collection) {
    // This is only enabled on the info collection GPU process.
    DevicePerfInfo device_perf_info;
    CollectDevicePerfInfo(&device_perf_info, /*in_browser_process=*/false);
    device_perf_info_ = device_perf_info;
  }

  if (!CanAccessDeviceFile(gpu_info_))
    return false;

  // GpuFeatureInfo is cached for the GPU service thread with WebView.
  if (!PopGpuFeatureInfoCache(&gpu_feature_info_)) {
    // Compute blocklist and driver bug workaround decisions based on basic GPU
    // info.
    gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                              command_line, &needs_more_info);
  }

#if defined(USE_EGL) && (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC))
  system_device_id = SetupGLDisplayManagerEGL(gpu_info_, gpu_feature_info_);
#endif  // USE_EGL && (IS_WIN || IS_MAC)
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CASTOS)

  gpu_info_.in_process_gpu = false;
  gl_use_swiftshader_ = false;

  // GL bindings may have already been initialized, specifically on MacOSX.
  bool gl_initialized = gl::GetGLImplementation() != gl::kGLImplementationNone;
  if (!gl_initialized) {
    // If GL has already been initialized, then it's too late to select GPU.
    if (SwitchableGPUsSupported(gpu_info_, *command_line)) {
      InitializeSwitchableGPUs(
          gpu_feature_info_.enabled_gpu_driver_bug_workarounds);
    }
  } else {
    // If SwiftShader/SwANGLE is in use, set the flag gl_use_swiftshader_ so GPU
    // initialization will take a software rendering path. Do not do this if
    // SwiftShader/SwANGLE are explicitly requested via flags, because the flags
    // are meant to specify running SwiftShader/SwANGLE on the hardware GPU
    // path.
    gl::GLImplementationParts impl = gl::GetGLImplementationParts();
    bool fallback_to_software_gl = false;
    absl::optional<gl::GLImplementationParts> requested_impl =
        gl::GetRequestedGLImplementationFromCommandLine(
            command_line, &fallback_to_software_gl);
    if (gl::IsSoftwareGLImplementation(impl) &&
        !(requested_impl && gl::IsSoftwareGLImplementation(*requested_impl))) {
      gl_use_swiftshader_ = true;
    }
  }

  bool enable_watchdog = !gpu_preferences_.disable_gpu_watchdog &&
                         !command_line->HasSwitch(switches::kHeadless) &&
                         !gl_use_swiftshader_;

  // Disable the watchdog in debug builds because they tend to only be run by
  // developers who will not appreciate the watchdog killing the GPU process.
#ifndef NDEBUG
  enable_watchdog = false;
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  bool gpu_sandbox_start_early = gpu_preferences_.gpu_sandbox_start_early;
#else   // !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
  // For some reasons MacOSX's VideoToolbox might crash when called after
  // initializing GL, see crbug.com/1047643 and crbug.com/871280. On other
  // operating systems like Windows and Android the pre-sandbox steps have
  // always been executed before initializing GL so keep it this way.
  bool gpu_sandbox_start_early = true;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  // PreSandbox is mainly for resource handling and not related to the GPU
  // driver, it doesn't need the GPU watchdog. The loadLibrary may take long
  // time that killing and restarting the GPU process will not help.
  if (gpu_sandbox_start_early) {
    // The sandbox will be started earlier than usual (i.e. before GL) so
    // execute the pre-sandbox steps now.
    sandbox_helper_->PreSandboxStartup(gpu_preferences);
  }

  // watchdog_init will call watchdog OnInitComplete() at the end of this
  // function.
  GpuWatchdogInit watchdog_init;

  // Don't start watchdog immediately, to allow developers to switch to VT2 on
  // startup.
  constexpr bool delayed_watchdog_enable = BUILDFLAG(IS_CHROMEOS_ASH);

  // Start the GPU watchdog only after anything that is expected to be time
  // consuming has completed, otherwise the process is liable to be aborted.
  if (enable_watchdog && !delayed_watchdog_enable) {
    watchdog_thread_ = GpuWatchdogThread::Create(
        gpu_preferences_.watchdog_starts_backgrounded, "GpuWatchdog");
    watchdog_init.SetGpuWatchdogPtr(watchdog_thread_.get());
  }

  bool attempted_startsandbox = false;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // On Chrome OS ARM Mali, GPU driver userspace creates threads when
  // initializing a GL context, so start the sandbox early.
  // TODO(zmo): Need to collect OS version before this.
  if (gpu_preferences_.gpu_sandbox_start_early) {
    gpu_info_.sandboxed = sandbox_helper_->EnsureSandboxInitialized(
        watchdog_thread_.get(), &gpu_info_, gpu_preferences_);
    attempted_startsandbox = true;
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  base::ElapsedTimer elapsed_timer;

#if BUILDFLAG(IS_OZONE)
  // Initialize Ozone GPU after the watchdog in case it hangs. The sandbox
  // may also have started at this point.
  ui::OzonePlatform::InitParams params;
  params.single_process = false;
  params.enable_native_gpu_memory_buffers =
      gpu_preferences_.enable_native_gpu_memory_buffers;

  // Page flip testing will only happen in ash-chrome, not in lacros-chrome.
  // Therefore, we only allow or disallow sync and real buffer page flip
  // testing for ash-chrome.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  params.allow_sync_and_real_buffer_page_flip_testing =
      gpu_preferences_.enable_chromeos_direct_video_decoder;
#else   // !BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  params.allow_sync_and_real_buffer_page_flip_testing = true;
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ui::OzonePlatform::InitializeForGPU(params);
  // We need to get supported formats before sandboxing to avoid an known
  // issue which breaks the camera preview. (b/166850715)
  std::vector<gfx::BufferFormat> supported_buffer_formats_for_texturing =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->GetSupportedFormatsForTexturing();
#endif  // BUILDFLAG(IS_OZONE)

  if (!gl_use_swiftshader_) {
    gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, needs_more_info);
  }

  if (gl_initialized && gl_use_swiftshader_ &&
      !gl::IsSoftwareGLImplementation(gl::GetGLImplementationParts())) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    VLOG(1) << "Quit GPU process launch to fallback to SwiftShader cleanly "
            << "on Linux";
    return false;
#else   // !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
    SaveHardwareGpuInfoAndGpuFeatureInfo();
    gl::init::ShutdownGL(nullptr, true);
    gl_initialized = false;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  }

  gl::GLDisplay* gl_display = nullptr;

  if (!gl_initialized) {
    // Pause watchdog. LoadLibrary in GLBindings may take long time.
    if (watchdog_thread_) {
      if (base::FeatureList::IsEnabled(
              features::kEnableWatchdogReportOnlyModeOnGpuInit)) {
        watchdog_thread_->EnableReportOnlyMode();
      } else {
        watchdog_thread_->PauseWatchdog();
      }
    }
    gl_initialized = gl::init::InitializeStaticGLBindingsOneOff();
    if (!gl_initialized) {
      VLOG(1) << "gl::init::InitializeStaticGLBindingsOneOff failed";
      return false;
    }
#if BUILDFLAG(IS_WIN)
    UMA_HISTOGRAM_BOOLEAN("GPU.AppHelpIsLoaded",
                          static_cast<bool>(::GetModuleHandle(L"apphelp.dll")));
#endif
    if (gl::GetGLImplementation() != gl::kGLImplementationDisabled) {
      gl_display = gl::init::InitializeGLNoExtensionsOneOff(
          /*init_bindings*/ false, system_device_id);
      gl_initialized = !!gl_display;
      if (!gl_initialized) {
        VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed";
        return false;
      }
    }
    if (watchdog_thread_) {
      if (base::FeatureList::IsEnabled(
              features::kEnableWatchdogReportOnlyModeOnGpuInit)) {
        watchdog_thread_->DisableReportOnlyMode();
      } else {
        watchdog_thread_->ResumeWatchdog();
      }
    }
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // The ContentSandboxHelper is currently the only one implementation of
  // GpuSandboxHelper and it has no dependency. Except on Linux where
  // VaapiWrapper checks the GL implementation to determine which display
  // to use. So call PreSandboxStartup after GL initialization. But make
  // sure the watchdog is paused as loadLibrary may take a long time and
  // restarting the GPU process will not help.
  if (!attempted_startsandbox) {
    if (watchdog_thread_)
      watchdog_thread_->PauseWatchdog();

    // The sandbox is not started yet.
    sandbox_helper_->PreSandboxStartup(gpu_preferences);

    if (watchdog_thread_)
      watchdog_thread_->ResumeWatchdog();
  }
#endif

  auto impl = gl::GetGLImplementationParts();
  bool gl_disabled = impl == gl::kGLImplementationDisabled;
  bool is_swangle = impl == gl::ANGLEImplementation::kSwiftShader;

  // Compute passthrough decoder status before ComputeGpuFeatureInfo below.
  // Do this after GL is initialized so extensions can be queried.
  // Using SwANGLE forces the passthrough command decoder.
  if (gpu_preferences_.use_passthrough_cmd_decoder || is_swangle) {
    gpu_info_.passthrough_cmd_decoder =
        gles2::PassthroughCommandDecoderSupported();
#if BUILDFLAG(IS_ANDROID)
    // We never use swiftshader on Android
    LOG_IF(DFATAL, !gpu_info_.passthrough_cmd_decoder)
#else
    LOG_IF(ERROR, !gpu_info_.passthrough_cmd_decoder)
#endif
        << "Passthrough is not supported, GL is "
        << gl::GetGLImplementationGLName(gl::GetGLImplementationParts())
        << ", ANGLE is "
        << gl::GetGLImplementationANGLEName(gl::GetGLImplementationParts());
  } else {
    gpu_info_.passthrough_cmd_decoder = false;
  }
  gpu_preferences_.use_passthrough_cmd_decoder =
      gpu_info_.passthrough_cmd_decoder;

  // We need to collect GL strings (VENDOR, RENDERER) for blocklisting purposes.
  if (!gl_disabled) {
    if (!gl_use_swiftshader_) {
      if (!CollectGraphicsInfo(&gpu_info_)) {
        VLOG(1) << "gpu::CollectGraphicsInfo failed";
        return false;
      }

      SetKeysForCrashLogging(gpu_info_);
      gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                                command_line, nullptr);
      gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
          command_line, gpu_feature_info_,
          gpu_preferences_.disable_software_rasterizer, false);
      if (gl_use_swiftshader_) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
        VLOG(1) << "Quit GPU process launch to fallback to SwiftShader cleanly "
                << "on Linux";
        return false;
#else
        SaveHardwareGpuInfoAndGpuFeatureInfo();
        gl::init::ShutdownGL(gl_display, true);
        watchdog_thread_ = nullptr;
        watchdog_init.SetGpuWatchdogPtr(nullptr);
        gl_display = gl::init::InitializeGLNoExtensionsOneOff(
            /*init_bindings=*/true, system_device_id);
        if (!gl_display) {
          VLOG(1)
              << "gl::init::InitializeGLNoExtensionsOneOff with SwiftShader "
              << "failed";
          return false;
        }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      }
    } else {  // gl_use_swiftshader_ == true
      switch (gpu_preferences_.use_vulkan) {
        case VulkanImplementationName::kNative: {
          // Collect GPU info, so we can use blocklist to disable vulkan if it
          // is needed.
          GPUInfo gpu_info;
          if (!CollectGraphicsInfo(&gpu_info)) {
            VLOG(1) << "gpu::CollectGraphicsInfo failed";
            return false;
          }
          auto gpu_feature_info = ComputeGpuFeatureInfo(
              gpu_info, gpu_preferences_, command_line, nullptr);
          gpu_feature_info_.status_values[GPU_FEATURE_TYPE_VULKAN] =
              gpu_feature_info.status_values[GPU_FEATURE_TYPE_VULKAN];
          break;
        }
        case VulkanImplementationName::kForcedNative:
        case VulkanImplementationName::kSwiftshader:
          gpu_feature_info_.status_values[GPU_FEATURE_TYPE_VULKAN] =
              kGpuFeatureStatusEnabled;
          break;
        case VulkanImplementationName::kNone:
          gpu_feature_info_.status_values[GPU_FEATURE_TYPE_VULKAN] =
              kGpuFeatureStatusDisabled;
          break;
      }
    }
  }

  // On MacOS, the default texture target for native GpuMemoryBuffers is
  // GL_TEXTURE_RECTANGLE_ARB. This is due to CGL's requirements for creating
  // a GL surface. However, when ANGLE is used on top of SwiftShader or Metal,
  // it's necessary to use GL_TEXTURE_2D instead.
  // TODO(crbug.com/1056312): The proper behavior is to check the config
  // parameter set by the EGL_ANGLE_iosurface_client_buffer extension
#if BUILDFLAG(IS_MAC)
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE &&
      (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader ||
       gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal)) {
    SetMacOSSpecificTextureTarget(GL_TEXTURE_2D);
    gpu_info_.macos_specific_texture_target = GL_TEXTURE_2D;
  }
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  {
    // On Windows, MITIGATION_FORCE_MS_SIGNED_BINS is used which disallows
    // loading any .dll that is not signed by Microsoft. Preload the SwiftShader
    // .dll so it may be accessed later. This is needed for WebGPU to
    // initialize a software fallback adapter.
    // Don't handle errors as failure here is non-fatal. Loading SwiftShader
    // again at a later point will fail as well.
    base::FilePath module_path;
    if (base::PathService::Get(base::DIR_MODULE, &module_path)) {
      base::LoadNativeLibrary(module_path.Append(L"vk_swiftshader.dll"),
                              nullptr);
    }
  }
#endif  // BUILDFLAG(IS_WIN)

  if (gpu_feature_info_.status_values[GPU_FEATURE_TYPE_VULKAN] !=
          kGpuFeatureStatusEnabled ||
      !InitializeVulkan()) {
    gpu_preferences_.use_vulkan = VulkanImplementationName::kNone;
    gpu_feature_info_.status_values[GPU_FEATURE_TYPE_VULKAN] =
        kGpuFeatureStatusDisabled;
    if (gpu_preferences_.gr_context_type == GrContextType::kVulkan) {
#if BUILDFLAG(IS_FUCHSIA)
      // Fuchsia uses ANGLE for GL which requires Vulkan, so don't fall
      // back to GL if Vulkan init fails.
      LOG(FATAL) << "Vulkan initialization failed";
#endif
      gpu_preferences_.gr_context_type = GrContextType::kGL;
    }
  } else {
    // TODO(https://crbug.com/1095744): It would be better to cleanly tear
    // down and recreate the VkDevice on VK_ERROR_DEVICE_LOST. Until that
    // happens, we will exit_on_context_lost to ensure there are no leaks.
    gpu_feature_info_.enabled_gpu_driver_bug_workarounds.push_back(
        EXIT_ON_CONTEXT_LOST);
  }

  // Collect GPU process info
  if (!gl_disabled) {
    if (!CollectGpuExtraInfo(&gpu_extra_info_, gpu_preferences)) {
      VLOG(1) << "gpu::CollectGpuExtraInfo failed";
      return false;
    }
  }

  if (!gl_disabled) {
    if (!gpu_feature_info_.disabled_extensions.empty()) {
      gl::init::SetDisabledExtensionsPlatform(
          gpu_feature_info_.disabled_extensions);
    }
    if (!gl::init::InitializeExtensionSettingsOneOffPlatform(gl_display)) {
      VLOG(1) << "gl::init::InitializeExtensionSettingsOneOffPlatform failed";
      return false;
    }
    default_offscreen_surface_ =
        gl::init::CreateOffscreenGLSurface(gl_display, gfx::Size());
    if (!default_offscreen_surface_) {
      VLOG(1) << "gl::init::CreateOffscreenGLSurface failed";
      return false;
    }
  }

  InitializePlatformOverlaySettings(&gpu_info_, gpu_feature_info_);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Driver may create a compatibility profile context when collect graphics
  // information on Linux platform. Try to collect graphics information
  // based on core profile context after disabling platform extensions.
  if (!gl_disabled && !gl_use_swiftshader_) {
    if (!CollectGraphicsInfo(&gpu_info_))
      return false;
    SetKeysForCrashLogging(gpu_info_);
    gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                              command_line, nullptr);
    gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, false);
    if (gl_use_swiftshader_) {
      VLOG(1) << "Quit GPU process launch to fallback to SwiftShader cleanly "
              << "on Linux";
      return false;
    }
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  if (gl_use_swiftshader_) {
    AdjustInfoToSwiftShader();
  }

  if (kGpuFeatureStatusEnabled !=
      gpu_feature_info_
          .status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE]) {
    gpu_preferences_.disable_accelerated_video_decode = true;
  }

  if (kGpuFeatureStatusEnabled !=
      gpu_feature_info_
          .status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE]) {
    gpu_preferences_.disable_accelerated_video_encode = true;
  }

  UMA_HISTOGRAM_MEDIUM_TIMES("GPU.InitializeOneOffMediumTime",
                             elapsed_timer.Elapsed());

  // SwANGLE is expected to run slowly, so disable the watchdog
  // in that case.
  if (!gl_use_swiftshader_ && command_line->HasSwitch(switches::kUseGL)) {
    std::string use_gl = command_line->GetSwitchValueASCII(switches::kUseGL);
    std::string use_angle =
        command_line->GetSwitchValueASCII(switches::kUseANGLE);
    if (use_gl == gl::kGLImplementationANGLEName &&
        (use_angle == gl::kANGLEImplementationSwiftShaderName ||
         use_angle == gl::kANGLEImplementationSwiftShaderForWebGLName)) {
      gl_use_swiftshader_ = true;
    }
  }
#if BUILDFLAG(IS_LINUX) || \
    (BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_DEVICE))
  if (!gl_disabled && !gl_use_swiftshader_ && std::getenv("RUNNING_UNDER_RR")) {
    // https://rr-project.org/ is a Linux-only record-and-replay debugger that
    // is unhappy when things like GPU drivers write directly into the
    // process's address space.  Using swiftshader helps ensure that doesn't
    // happen and keeps Chrome and linux-chromeos usable with rr.
    gl_use_swiftshader_ = true;
  }
#endif
  if (gl_use_swiftshader_ ||
      gl::IsSoftwareGLImplementation(gl::GetGLImplementationParts())) {
    gpu_info_.software_rendering = true;
    watchdog_thread_ = nullptr;
    watchdog_init.SetGpuWatchdogPtr(nullptr);
  } else if (gl_disabled) {
    watchdog_thread_ = nullptr;
    watchdog_init.SetGpuWatchdogPtr(nullptr);
  } else if (enable_watchdog && delayed_watchdog_enable) {
    watchdog_thread_ = GpuWatchdogThread::Create(
        gpu_preferences_.watchdog_starts_backgrounded, "GpuWatchdog");
    watchdog_init.SetGpuWatchdogPtr(watchdog_thread_.get());
  }

  UMA_HISTOGRAM_ENUMERATION("GPU.GLImplementation", gl::GetGLImplementation());

  if (!gpu_info_.sandboxed && !attempted_startsandbox) {
    gpu_info_.sandboxed = sandbox_helper_->EnsureSandboxInitialized(
        watchdog_thread_.get(), &gpu_info_, gpu_preferences_);
  }

  init_successful_ = true;
#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::GetInstance()->AfterSandboxEntry();
  gpu_feature_info_.supported_buffer_formats_for_allocation_and_texturing =
      std::move(supported_buffer_formats_for_texturing);
#endif

  if (!watchdog_thread_)
    watchdog_init.SetGpuWatchdogPtr(nullptr);

#if BUILDFLAG(IS_WIN)
  if (gpu_feature_info_.IsWorkaroundEnabled(DISABLE_DECODE_SWAP_CHAIN))
    gl::DisableDirectCompositionDecodeSwapChain();
  if (gpu_feature_info_.IsWorkaroundEnabled(
          DISABLE_DIRECT_COMPOSITION_SW_VIDEO_OVERLAYS)) {
    gl::DisableDirectCompositionSoftwareOverlays();
  }
#endif

#if defined(USE_EGL) && !BUILDFLAG(IS_MAC)
  if (gpu_feature_info_.IsWorkaroundEnabled(CHECK_EGL_FENCE_BEFORE_WAIT))
    gl::GLFenceEGL::CheckEGLFenceBeforeWait();

  if (gpu_feature_info_.IsWorkaroundEnabled(FLUSH_BEFORE_CREATE_FENCE))
    gl::GLFenceEGL::FlushBeforeCreateFence();
#endif

  return true;
}

#if BUILDFLAG(IS_ANDROID)
void GpuInit::InitializeInProcess(base::CommandLine* command_line,
                                  const GpuPreferences& gpu_preferences) {
  gpu_preferences_ = gpu_preferences;
  init_successful_ = true;
  DCHECK(!EnableSwiftShaderIfNeeded(
      command_line, gpu_feature_info_,
      gpu_preferences_.disable_software_rasterizer, false));

  gl::GLDisplay* gl_display = InitializeGLThreadSafe(
      command_line, gpu_preferences_, &gpu_info_, &gpu_feature_info_);

  if (command_line->HasSwitch(switches::kWebViewDrawFunctorUsesVulkan) &&
      base::FeatureList::IsEnabled(features::kWebViewVulkan)) {
    bool result = InitializeVulkan();
    // There is no fallback for webview.
    CHECK(result);
  } else {
    DisableInProcessGpuVulkan(&gpu_feature_info_, &gpu_preferences_);
  }

  default_offscreen_surface_ =
      gl::init::CreateOffscreenGLSurface(gl_display, gfx::Size());

  UMA_HISTOGRAM_ENUMERATION("GPU.GLImplementation", gl::GetGLImplementation());
}
#else
void GpuInit::InitializeInProcess(base::CommandLine* command_line,
                                  const GpuPreferences& gpu_preferences) {
  gpu_preferences_ = gpu_preferences;
  init_successful_ = true;
#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;

  // Page flip testing will only happen in ash-chrome, not in lacros-chrome.
  // Therefore, we only allow or disallow sync and real buffer page flip
  // testing for ash-chrome.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  params.allow_sync_and_real_buffer_page_flip_testing =
      gpu_preferences_.enable_chromeos_direct_video_decoder;
#else   // !BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  params.allow_sync_and_real_buffer_page_flip_testing = true;
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ui::OzonePlatform::InitializeForGPU(params);
#endif
  bool needs_more_info = true;
#if !BUILDFLAG(IS_CASTOS) && !BUILDFLAG(IS_CAST_ANDROID)
  needs_more_info = false;
  if (!PopGPUInfoCache(&gpu_info_)) {
    CollectBasicGraphicsInfo(command_line, &gpu_info_);
  }
#if defined(SUBPIXEL_FONT_RENDERING_DISABLED)
  gpu_info_.subpixel_font_rendering = false;
#else
  gpu_info_.subpixel_font_rendering = true;
#endif
  if (!PopGpuFeatureInfoCache(&gpu_feature_info_)) {
    gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                              command_line, &needs_more_info);
  }
  if (SwitchableGPUsSupported(gpu_info_, *command_line)) {
    InitializeSwitchableGPUs(
        gpu_feature_info_.enabled_gpu_driver_bug_workarounds);
  }
#endif  // !BUILDFLAG(IS_CASTOS) && !BUILDFLAG(IS_CAST_ANDROID)

  gl::GLDisplay* gl_display = nullptr;

  gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
      command_line, gpu_feature_info_,
      gpu_preferences_.disable_software_rasterizer, needs_more_info);
  gl_display = gl::init::InitializeGLNoExtensionsOneOff(/*init_bindings=*/true,
                                                        /*system_device_id=*/0);
  if (!gl_display) {
    VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed";
    return;
  }
  bool gl_disabled = gl::GetGLImplementation() == gl::kGLImplementationDisabled;

#if BUILDFLAG(IS_LINUX) || \
    (BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_DEVICE))
  if (!gl_disabled && !gl_use_swiftshader_ && std::getenv("RUNNING_UNDER_RR")) {
    // https://rr-project.org/ is a Linux-only record-and-replay debugger that
    // is unhappy when things like GPU drivers write directly into the
    // process's address space.  Using swiftshader helps ensure that doesn't
    // happen and keeps Chrome and linux-chromeos usable with rr.
    gl_use_swiftshader_ = true;
  }
#endif

  if (!gl_disabled && !gl_use_swiftshader_) {
    CollectContextGraphicsInfo(&gpu_info_);
    gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                              command_line, nullptr);
    gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, false);
    if (gl_use_swiftshader_) {
      SaveHardwareGpuInfoAndGpuFeatureInfo();
      gl::init::ShutdownGL(gl_display, true);
      gl_display = gl::init::InitializeGLNoExtensionsOneOff(
          /*init_bindings=*/true, /*system_device_id=*/0);
      if (!gl_display) {
        VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed "
                << "with SwiftShader";
        return;
      }
    }
  }

  // On MacOS, the default texture target for native GpuMemoryBuffers is
  // GL_TEXTURE_RECTANGLE_ARB. This is due to CGL's requirements for creating
  // a GL surface. However, when ANGLE is used on top of SwiftShader or Metal,
  // it's necessary to use GL_TEXTURE_2D instead.
  // TODO(crbug.com/1056312): The proper behavior is to check the config
  // parameter set by the EGL_ANGLE_iosurface_client_buffer extension
#if BUILDFLAG(IS_MAC)
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE &&
      (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader ||
       gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal)) {
    SetMacOSSpecificTextureTarget(GL_TEXTURE_2D);
    gpu_info_.macos_specific_texture_target = GL_TEXTURE_2D;
  }
#endif  // BUILDFLAG(IS_MAC)

  if (!gl_disabled) {
    if (!gpu_feature_info_.disabled_extensions.empty()) {
      gl::init::SetDisabledExtensionsPlatform(
          gpu_feature_info_.disabled_extensions);
    }
    if (!gl::init::InitializeExtensionSettingsOneOffPlatform(gl_display)) {
      VLOG(1) << "gl::init::InitializeExtensionSettingsOneOffPlatform failed";
    }
    default_offscreen_surface_ =
        gl::init::CreateOffscreenGLSurface(gl_display, gfx::Size());
    if (!default_offscreen_surface_) {
      VLOG(1) << "gl::init::CreateOffscreenGLSurface failed";
    }
  }

  InitializePlatformOverlaySettings(&gpu_info_, gpu_feature_info_);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Driver may create a compatibility profile context when collect graphics
  // information on Linux platform. Try to collect graphics information
  // based on core profile context after disabling platform extensions.
  if (!gl_disabled && !gl_use_swiftshader_) {
    CollectContextGraphicsInfo(&gpu_info_);
    gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                              command_line, nullptr);
    gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, false);
    if (gl_use_swiftshader_) {
      SaveHardwareGpuInfoAndGpuFeatureInfo();
      gl::init::ShutdownGL(gl_display, true);
      gl_display = gl::init::InitializeGLNoExtensionsOneOff(
          /*init_bindings=*/true, /*system_device_id=*/0);
      if (!gl_display) {
        VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed "
                << "with SwiftShader";
        return;
      }
    }
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  if (gl_use_swiftshader_) {
    AdjustInfoToSwiftShader();
  }

#if BUILDFLAG(IS_OZONE)
  const std::vector<gfx::BufferFormat> supported_buffer_formats_for_texturing =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->GetSupportedFormatsForTexturing();
  gpu_feature_info_.supported_buffer_formats_for_allocation_and_texturing =
      std::move(supported_buffer_formats_for_texturing);
#endif

  DisableInProcessGpuVulkan(&gpu_feature_info_, &gpu_preferences_);

#if BUILDFLAG(IS_WIN)
  if (gpu_feature_info_.IsWorkaroundEnabled(DISABLE_DECODE_SWAP_CHAIN))
    gl::DisableDirectCompositionDecodeSwapChain();
#endif

  UMA_HISTOGRAM_ENUMERATION("GPU.GLImplementation", gl::GetGLImplementation());
}
#endif  // BUILDFLAG(IS_ANDROID)

void GpuInit::SaveHardwareGpuInfoAndGpuFeatureInfo() {
  gpu_info_for_hardware_gpu_ = gpu_info_;
  gpu_feature_info_for_hardware_gpu_ = gpu_feature_info_;
}

void GpuInit::AdjustInfoToSwiftShader() {
  gpu_info_.passthrough_cmd_decoder = false;
  gpu_feature_info_ = ComputeGpuFeatureInfoForSwiftShader();
  CollectContextGraphicsInfo(&gpu_info_);

  DCHECK_EQ(gpu_info_.passthrough_cmd_decoder, false);
}

scoped_refptr<gl::GLSurface> GpuInit::TakeDefaultOffscreenSurface() {
  return std::move(default_offscreen_surface_);
}

bool GpuInit::InitializeVulkan() {
#if BUILDFLAG(ENABLE_VULKAN)
  DCHECK_EQ(gpu_feature_info_.status_values[GPU_FEATURE_TYPE_VULKAN],
            kGpuFeatureStatusEnabled);
  DCHECK_NE(gpu_preferences_.use_vulkan, VulkanImplementationName::kNone);
  bool vulkan_use_swiftshader =
      gpu_preferences_.use_vulkan == VulkanImplementationName::kSwiftshader;
  bool forced_native =
      gpu_preferences_.use_vulkan == VulkanImplementationName::kForcedNative;
  bool use_swiftshader = gl_use_swiftshader_ || vulkan_use_swiftshader;

  vulkan_implementation_ = CreateVulkanImplementation(
      vulkan_use_swiftshader, gpu_preferences_.enable_vulkan_protected_memory);
  if (!vulkan_implementation_ ||
      !vulkan_implementation_->InitializeVulkanInstance(
          !gpu_preferences_.disable_vulkan_surface)) {
    DLOG(ERROR) << "Failed to create and initialize Vulkan implementation.";
    vulkan_implementation_ = nullptr;
    CHECK(!gpu_preferences_.disable_vulkan_fallback_to_gl_for_testing);
  }

  // Vulkan info is no longer collected in gpu/config/gpu_info_collector_win.cc
  // Histogram GPU.SupportsVulkan and GPU.VulkanVersion were marked as expired.
  // TODO(magchen): Add back these two histograms here and re-enable them in
  // histograms.xml when we start Vulkan finch on Windows.

  if (!vulkan_implementation_)
    return false;

  const base::FeatureParam<std::string> disable_patterns(
      &features::kVulkan, "disable_by_gl_renderer",
      "*Mali-G?? M*" /* https://crbug.com/1183702 */);
  if (MatchGLInfo(gpu_info_.gl_renderer, disable_patterns.Get()))
    return false;

  const base::FeatureParam<std::string> disable_driver_patterns(
      &features::kVulkan, "disable_by_gl_driver",
#if BUILDFLAG(IS_ANDROID)
      "324.0|331.0|334.0|378.0|415.0|420.0|444.0" /* https://crbug.com/1246857
                                                   */
#else
      ""
#endif
  );
  if (MatchGLInfo(gpu_info_.gpu.driver_version, disable_driver_patterns.Get()))
    return false;

  const base::FeatureParam<std::string> force_enable_patterns(
      &features::kVulkan, "force_enable_by_gl_renderer", "");
  forced_native |=
      MatchGLInfo(gpu_info_.gl_renderer, force_enable_patterns.Get());

  const base::FeatureParam<std::string> enable_by_device_name(
      &features::kVulkan, "enable_by_device_name", "");
  if (!use_swiftshader && !forced_native &&
      !CheckVulkanCompabilities(
          vulkan_implementation_->GetVulkanInstance()->vulkan_info(), gpu_info_,
          enable_by_device_name.Get())) {
    vulkan_implementation_.reset();
    return false;
  }

  gpu_info_.vulkan_info =
      vulkan_implementation_->GetVulkanInstance()->vulkan_info();
  return true;
#else   // !BUILDFLAG(ENABLE_VULKAN)
  return false;
#endif  // BUILDFLAG(ENABLE_VULKAN)
}

}  // namespace gpu

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_init.h"

#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
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
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_switching.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
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
#include "gpu/vulkan/drm_modifiers_filter_vulkan.h"
#include "ui/ozone/public/drm_modifiers_filter.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_surface_egl.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "ui/gfx/android/android_surface_control_compat.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_util.h"
#endif

#if !BUILDFLAG(IS_MAC)
#include "ui/gl/gl_fence_egl.h"
#endif

#if BUILDFLAG(USE_DAWN) || BUILDFLAG(SKIA_USE_DAWN)
#include "third_party/dawn/include/dawn/dawn_proc.h"          // nogncheck
#include "third_party/dawn/include/dawn/native/DawnNative.h"  // nogncheck
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"  // nogncheck
#endif

#if BUILDFLAG(SKIA_USE_DAWN) && BUILDFLAG(IS_CHROMEOS)
#include "gpu/command_buffer/service/drm_modifiers_filter_dawn.h"
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

void InitializeDawnProcs() {
#if BUILDFLAG(USE_DAWN) || BUILDFLAG(SKIA_USE_DAWN)
  // Setup the global procs table for GPU process.
  dawnProcSetProcs(&dawn::native::GetProcs());
#endif  // BUILDFLAG(USE_DAWN) || BUILDFLAG(SKIA_USE_DAWN)
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
      .disable_sw_video_overlays = gpu_feature_info.IsWorkaroundEnabled(
          DISABLE_DIRECT_COMPOSITION_SW_VIDEO_OVERLAYS),
      .disable_decode_swap_chain =
          gpu_feature_info.IsWorkaroundEnabled(DISABLE_DECODE_SWAP_CHAIN),
      .enable_bgra8_overlays_with_yuv_overlay_support =
          gpu_feature_info.IsWorkaroundEnabled(
              gpu::ENABLE_BGRA8_OVERLAYS_WITH_YUV_OVERLAY_SUPPORT),
      .force_nv12_overlay_support =
          gpu_feature_info.IsWorkaroundEnabled(gpu::FORCE_NV12_OVERLAY_SUPPORT),
      .force_rgb10a2_overlay_support = gpu_feature_info.IsWorkaroundEnabled(
          gpu::FORCE_RGB10A2_OVERLAY_SUPPORT),
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
  raw_ptr<GpuWatchdogThread, DanglingUntriaged> watchdog_ptr_ = nullptr;
};

void PauseGpuWatchdog(GpuWatchdogThread* watchdog_thread) {
  if (watchdog_thread) {
    watchdog_thread->PauseWatchdog();
  }
}
void ResumeGpuWatchdog(GpuWatchdogThread* watchdog_thread) {
  if (watchdog_thread) {
    watchdog_thread->ResumeWatchdog();
  }
}

// TODO(crbug.com/40700374): We currently do not handle
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

#if BUILDFLAG(IS_ANDROID)
// TODO(https://crbug.com/324468229): We currently do not handle Dawn device
// lost with in-process-gpu.
void DisableInProcessGpuGraphite(GpuFeatureInfo& gpu_feature_info,
                                 GpuPreferences& gpu_preferences) {
  if (gpu_feature_info.status_values[GPU_FEATURE_TYPE_SKIA_GRAPHITE] ==
          kGpuFeatureStatusEnabled ||
      gpu_preferences.gr_context_type == GrContextType::kGraphiteDawn) {
    LOG(ERROR) << "Graphite not supported with in process gpu";
    gpu_feature_info.status_values[GPU_FEATURE_TYPE_SKIA_GRAPHITE] =
        kGpuFeatureStatusDisabled;
    gpu_preferences.gr_context_type = GrContextType::kGL;
  }
}
#endif

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// GPU picking is only effective with ANGLE/Metal backend on Mac and
// on Windows with EGL.
// Returns the default GPU's system_device_id.
void SetupGLDisplayManagerEGL(const GPUInfo& gpu_info,
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
      gpu_high_perf ? gpu_high_perf->system_device_id : 0;
  uint64_t system_device_id_low_power =
      gpu_low_power ? gpu_low_power->system_device_id : 0;
  uint64_t system_device_id_default = gpu_default->system_device_id;
#endif
  DCHECK(gpu_default);

  if (gpu_info.GpuCount() <= 1) {
    gl::SetGpuPreferenceEGL(gl::GpuPreference::kDefault,
                            system_device_id_default);
    return;
  }
  if (gpu_feature_info.IsWorkaroundEnabled(FORCE_LOW_POWER_GPU) &&
      system_device_id_low_power) {
    gl::SetGpuPreferenceEGL(gl::GpuPreference::kDefault,
                            system_device_id_low_power);
    return;
  }
  if (gpu_feature_info.IsWorkaroundEnabled(FORCE_HIGH_PERFORMANCE_GPU) &&
      system_device_id_high_perf) {
    gl::SetGpuPreferenceEGL(gl::GpuPreference::kDefault,
                            system_device_id_high_perf);
    return;
  }
  if (gpu_default == gpu_high_perf) {
    // If the default GPU is already the high performance GPU, then it's better
    // for Chrome to always use this GPU.
    gl::SetGpuPreferenceEGL(gl::GpuPreference::kDefault,
                            system_device_id_high_perf);
    return;
  }

  // Chrome uses the default GPU for internal rendering and the high
  // performance GPU for WebGL/WebGPU contexts that prefer high performance.
  // At this moment, a low power GPU different from the default GPU is not
  // supported.
  gl::SetGpuPreferenceEGL(gl::GpuPreference::kDefault,
                          system_device_id_default);
  if (system_device_id_high_perf) {
    gl::SetGpuPreferenceEGL(gl::GpuPreference::kHighPerformance,
                            system_device_id_high_perf);
  }
  return;
}
#endif  // IS_WIN || IS_MAC

}  // namespace

GpuInit::GpuInit() = default;

GpuInit::~GpuInit() {
  StopForceDiscreteGPU();
}

bool GpuInit::InitializeAndStartSandbox(base::CommandLine* command_line,
                                        const GpuPreferences& gpu_preferences) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  LOG(WARNING) << "Starting gpu initialization.";
#endif
  gpu_preferences_ = gpu_preferences;
  // Blocklist decisions based on basic GPUInfo may not be final. It might
  // need more context based GPUInfo. In such situations, switching to
  // SwiftShader needs to wait until creating a context.
  bool needs_more_info = true;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CASTOS)
  needs_more_info = false;
  CollectBasicGraphicsInfo(command_line, &gpu_info_);

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

  // Compute blocklist and driver bug workaround decisions based on basic GPU
  // info.
  gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                            command_line, &needs_more_info);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  SetupGLDisplayManagerEGL(gpu_info_, gpu_feature_info_);
#endif  // IS_WIN || IS_MAC
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CASTOS)

  gpu_info_.in_process_gpu = false;

  DCHECK_EQ(gl::GetGLImplementation(), gl::kGLImplementationNone);
  if (SwitchableGPUsSupported(gpu_info_, *command_line)) {
    InitializeSwitchableGPUs(
        gpu_feature_info_.enabled_gpu_driver_bug_workarounds);
  }
  gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
      command_line, gpu_feature_info_,
      gpu_preferences_.disable_software_rasterizer, needs_more_info);

  bool enable_watchdog = !gpu_preferences_.disable_gpu_watchdog &&
                         !command_line->HasSwitch(switches::kHeadless);

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
    watchdog_thread_ =
        GpuWatchdogThread::Create(gpu_preferences_.watchdog_starts_backgrounded,
                                  gl_use_swiftshader_, "GpuWatchdog");
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
  params.handle_overlays_swap_failure =
      base::FeatureList::IsEnabled(features::kHandleOverlaysSwapFailure);

  // Page flip testing will only happen in ash-chrome, not in lacros-chrome.
  // Therefore, we only allow or disallow sync and real buffer page flip
  // testing for ash-chrome.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  params.allow_sync_and_real_buffer_page_flip_testing = true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ui::OzonePlatform::InitializeForGPU(params);
#endif  // BUILDFLAG(IS_OZONE)

  gl::GLDisplay* gl_display = nullptr;

  // Pause watchdog. LoadLibrary in GLBindings may take long time.
  PauseGpuWatchdog(watchdog_thread_.get());

  if (!gl::init::InitializeStaticGLBindingsOneOff()) {
    VLOG(1) << "gl::init::InitializeStaticGLBindingsOneOff failed";
    return false;
  }
#if BUILDFLAG(IS_WIN)
  UMA_HISTOGRAM_BOOLEAN("GPU.AppHelpIsLoaded",
                        static_cast<bool>(::GetModuleHandle(L"apphelp.dll")));
  if (gpu_preferences_.gr_context_type == GrContextType::kGraphiteDawn &&
      features::kSkiaGraphiteDawnBackendValidation.Get()) {
    // Enable ANGLE debug layer if we need backend validation for Graphite since
    // we can share the D3D11 device between ANGLE and Dawn.
    gl::GLDisplayEGL::EnableANGLEDebugLayer();
  }
#endif
  if (gl::GetGLImplementation() != gl::kGLImplementationDisabled) {
    gl_display = gl::init::InitializeGLNoExtensionsOneOff(
        /*init_bindings*/ false, gl::GpuPreference::kDefault);
    if (!gl_display) {
      // If GL initialization failed, GPU process will be teardown later, sp set
      // gpu_preferences_.gr_context_type to kGL to avoid initializing
      // DawnContextProvider later.
      gpu_preferences_.gr_context_type = GrContextType::kGL;
      VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed";
      return false;
    }
  }

  const bool need_fallback_from_graphite = [this]() {
    // If graphite is requested, check ANGLE implementation.
    if (gpu_preferences_.gr_context_type != GrContextType::kGraphiteDawn &&
        gpu_preferences_.gr_context_type != GrContextType::kGraphiteMetal) {
      return false;
    }

#if BUILDFLAG(IS_APPLE)
    // Graphite requires ANGLE Metal (or Swiftshader, handled below) on Mac
    constexpr auto kRequiredANGLEImplementation = gl::ANGLEImplementation::kMetal;
#elif BUILDFLAG(IS_WIN)
    // Graphite requires ANGLE D3D11 (or Swiftshader, handled below) on Windows
    constexpr auto kRequiredANGLEImplementation = gl::ANGLEImplementation::kD3D11;
#else
    constexpr auto kRequiredANGLEImplementation = gl::ANGLEImplementation::kNone;
#endif
    if (kRequiredANGLEImplementation == gl::ANGLEImplementation::kNone ||
        gl::GetANGLEImplementation() == kRequiredANGLEImplementation) {
      // If ANGLE is using required implementation, fallback is not needed.
      return false;
    }

    // If ANGLE is using Swiftshader, fallback is not needed.
    if (gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader) {
      return false;
    }

    // If graphite is requested from command line, fallback is not needed.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableSkiaGraphite)) {
      return false;
    }

    return true;
  }();

  if (need_fallback_from_graphite) {
    gpu_preferences_.gr_context_type = GrContextType::kGL;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // The ContentSandboxHelper is currently the only one implementation of
  // GpuSandboxHelper and it has no dependency. Except on Linux where
  // VaapiWrapper checks the GL implementation to determine which display
  // to use. So call PreSandboxStartup after GL initialization. But make
  // sure the watchdog is paused as loadLibrary may take a long time and
  // restarting the GPU process will not help.
  if (!attempted_startsandbox) {
    // The sandbox is not started yet.
    sandbox_helper_->PreSandboxStartup(gpu_preferences);
  }
#endif

  ResumeGpuWatchdog(watchdog_thread_.get());

  auto impl = gl::GetGLImplementationParts();
  bool gl_disabled = impl == gl::kGLImplementationDisabled;

#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
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
#else
  // If gl is disabled passthrough/validating command decoder doesn't matter. If
  // it's not ensure that passthrough command decoder is supported as it's our
  // only option.
  if (!gl_disabled) {
    LOG_IF(FATAL, !gles2::PassthroughCommandDecoderSupported())
        << "Passthrough is not supported, GL is "
        << gl::GetGLImplementationGLName(gl::GetGLImplementationParts())
        << ", ANGLE is "
        << gl::GetGLImplementationANGLEName(gl::GetGLImplementationParts());
    gpu_info_.passthrough_cmd_decoder = true;
    gpu_preferences_.use_passthrough_cmd_decoder = true;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // TODO(b/233238923): While passthrough is rolling out on CrOS, it's useful
  // to know whether a bug report is for a session with passthrough enabled.
  // Remove this logging when passthrough is fully launched on CrOS.
  if (gpu_preferences_.use_passthrough_cmd_decoder) {
    LOG(WARNING) << "Using passthrough command decoder. NOTE: This log is "
        << "to help triage feedback reports and does not by itself mean there "
        << "is an issue.";
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

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
        if (watchdog_thread_.get()) {
          // Recreate watchdog for software rasterizer.
          watchdog_thread_ = nullptr;
          watchdog_init.SetGpuWatchdogPtr(nullptr);
          watchdog_thread_ = GpuWatchdogThread::Create(
              gpu_preferences_.watchdog_starts_backgrounded,
              /*software_rendering=*/true, "GpuWatchdog");
          watchdog_init.SetGpuWatchdogPtr(watchdog_thread_.get());
        }
        gl_display = gl::init::InitializeGLNoExtensionsOneOff(
            /*init_bindings=*/true, gl::GpuPreference::kDefault);
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

#if BUILDFLAG(IS_WIN)
  {
    // On Windows, MITIGATION_FORCE_MS_SIGNED_BINS is used which disallows
    // loading any .dll that is not signed by Microsoft. Preload the SwiftShader
    // .dll so it may be accessed later. This is needed for WebGPU to
    // initialize a software fallback adapter. Also do the same for DXC,
    // which WebGPU may use on D3D12 devices.
    // Don't handle errors as failure here is non-fatal. Loading either DLL
    // again at a later point will fail as well.
    PauseGpuWatchdog(watchdog_thread_.get());

    base::FilePath module_path;
    if (base::PathService::Get(base::DIR_MODULE, &module_path)) {
      base::LoadNativeLibrary(module_path.Append(L"vk_swiftshader.dll"),
                              nullptr);

#if defined(DAWN_USE_BUILT_DXC)
      // TODO(crbug.com/40075751): Preload dxil.dll to avoid loader lock issues
      // since dxcompiler.dll loads dxil.dll from DllMain.
      base::LoadNativeLibrary(module_path.Append(L"dxil.dll"), nullptr);
      base::LoadNativeLibrary(module_path.Append(L"dxcompiler.dll"), nullptr);
#endif

      // Preload a redistributable DirectML.dll that allows testing WebNN
      // against newer release of DirectML before it is integrated into
      // Windows OS. Don't handle errors as failure here is non-fatal. The
      // DirectML.dll within system folder will be loaded at a later point if
      // the redistributable one fails to be loaded.
      if (command_line->HasSwitch(switches::kUseRedistributableDirectML)) {
        base::LoadNativeLibrary(module_path.Append(L"directml.dll"), nullptr);
      }
    }

    ResumeGpuWatchdog(watchdog_thread_.get());
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
#else
      gpu_preferences_.gr_context_type = GrContextType::kGL;
#endif
    }
  } else {
    // TODO(crbug.com/40700374): It would be better to cleanly tear
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

#if BUILDFLAG(IS_OZONE)
  // We need to get supported formats before sandboxing to avoid an known
  // issue which breaks the camera preview. (b/166850715)
  std::vector<gfx::BufferFormat> supported_buffer_formats_for_texturing =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->GetSupportedFormatsForTexturing();
  std::vector<gfx::BufferFormat>
      supported_buffer_formats_for_gl_native_pixmap_import =
          ui::OzonePlatform::GetInstance()
              ->GetSurfaceFactoryOzone()
              ->GetSupportedFormatsForGLNativePixmapImport();
#endif  // BUILDFLAG(IS_OZONE)

  InitializePlatformOverlaySettings(&gpu_info_, gpu_feature_info_);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Driver may create a compatibility profile context when collect graphics
  // information on Linux platform. Try to collect graphics information
  // based on core profile context after disabling platform extensions.
  if (!gl_disabled && !gl_use_swiftshader_) {
    if (!CollectGraphicsInfo(&gpu_info_)) {
      return false;
    }
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

  bool recreate_watchdog = false;
  if (!gl_use_swiftshader_ && command_line->HasSwitch(switches::kUseGL)) {
    std::string use_gl = command_line->GetSwitchValueASCII(switches::kUseGL);
    std::string use_angle =
        command_line->GetSwitchValueASCII(switches::kUseANGLE);
    if (use_gl == gl::kGLImplementationANGLEName &&
        (use_angle == gl::kANGLEImplementationSwiftShaderName ||
         use_angle == gl::kANGLEImplementationSwiftShaderForWebGLName)) {
      gl_use_swiftshader_ = true;
      if (watchdog_thread_) {
        recreate_watchdog = true;
      }
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
    if (watchdog_thread_) {
      recreate_watchdog = true;
    }
  }
#endif
  gpu_info_.gl_implementation_parts = gl::GetGLImplementationParts();
  bool software_rendering = false;
  if (gl_use_swiftshader_ ||
      gl::IsSoftwareGLImplementation(gl::GetGLImplementationParts())) {
    software_rendering = true;
  } else if (gl_disabled) {
    DCHECK(!recreate_watchdog);
    watchdog_thread_ = nullptr;
    watchdog_init.SetGpuWatchdogPtr(nullptr);
  } else if (enable_watchdog && delayed_watchdog_enable) {
    recreate_watchdog = true;
  }
  if (recreate_watchdog) {
    watchdog_thread_ = nullptr;
    watchdog_init.SetGpuWatchdogPtr(nullptr);
    watchdog_thread_ =
        GpuWatchdogThread::Create(gpu_preferences_.watchdog_starts_backgrounded,
                                  software_rendering, "GpuWatchdog");
    watchdog_init.SetGpuWatchdogPtr(watchdog_thread_.get());
  }

  UMA_HISTOGRAM_ENUMERATION("GPU.GLImplementation", gl::GetGLImplementation());

  if (!gpu_info_.sandboxed && !attempted_startsandbox) {
    gpu_info_.sandboxed = sandbox_helper_->EnsureSandboxInitialized(
        watchdog_thread_.get(), &gpu_info_, gpu_preferences_);
  }
  UMA_HISTOGRAM_BOOLEAN("GPU.Sandboxed", gpu_info_.sandboxed);

  InitializeDawnProcs();

  if (gpu_preferences_.gr_context_type == GrContextType::kGraphiteDawn) {
    if (!InitializeDawn()) {
      if (gpu_feature_info_.status_values[GPU_FEATURE_TYPE_SKIA_GRAPHITE] ==
          kGpuFeatureStatusEnabled) {
        return false;
      }
      // SkiaGraphite is disabled by software_rendering_list.json
      gpu_preferences_.gr_context_type = GrContextType::kGL;
    }
  }

  init_successful_ = true;
#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::GetInstance()->AfterSandboxEntry();
  gpu_feature_info_.supported_buffer_formats_for_allocation_and_texturing =
      std::move(supported_buffer_formats_for_texturing);
  gpu_feature_info_.supported_buffer_formats_for_gl_native_pixmap_import =
      std::move(supported_buffer_formats_for_gl_native_pixmap_import);
  [[maybe_unused]] auto* factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  bool filter_set = false;
#if BUILDFLAG(ENABLE_VULKAN)
  if (gpu_feature_info_.status_values[GPU_FEATURE_TYPE_VULKAN] ==
          kGpuFeatureStatusEnabled &&
      factory->SupportsDrmModifiersFilter()) {
    CHECK(!filter_set);
    DCHECK(vulkan_implementation_ &&
           vulkan_implementation_->GetVulkanInstance() &&
           vulkan_implementation_->GetVulkanInstance()->vk_instance() !=
               VK_NULL_HANDLE);
    factory->SetDrmModifiersFilter(std::make_unique<DrmModifiersFilterVulkan>(
        vulkan_implementation_.get()));
    filter_set = true;
  }
#endif  // BUILDFLAG(ENABLE_VULKAN)
#if BUILDFLAG(SKIA_USE_DAWN) && BUILDFLAG(IS_CHROMEOS)
  if (dawn_context_provider_ && factory->SupportsDrmModifiersFilter()) {
    CHECK(!filter_set);
    factory->SetDrmModifiersFilter(std::make_unique<DrmModifiersFilterDawn>(
        dawn_context_provider_->GetDevice().GetAdapter()));
    filter_set = true;
  }
#endif  // BUILDFLAG(SKIA_USE_DAWN) && BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(IS_OZONE)

  if (!watchdog_thread_) {
    watchdog_init.SetGpuWatchdogPtr(nullptr);
  }

#if !BUILDFLAG(IS_MAC)
  if (gpu_feature_info_.IsWorkaroundEnabled(CHECK_EGL_FENCE_BEFORE_WAIT)) {
    gl::GLFenceEGL::CheckEGLFenceBeforeWait();
  }

  if (gpu_feature_info_.IsWorkaroundEnabled(FLUSH_BEFORE_CREATE_FENCE)) {
    gl::GLFenceEGL::FlushBeforeCreateFence();
  }
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

  if (!gl_display) {
    LOG(FATAL) << "gpu::InitializeGLThreadSafe() failed.";
  }

  if (command_line->HasSwitch(switches::kWebViewDrawFunctorUsesVulkan)) {
    bool result = InitializeVulkan();
    // There is no fallback for webview.
    CHECK(result);
  } else {
    DisableInProcessGpuVulkan(&gpu_feature_info_, &gpu_preferences_);
    DisableInProcessGpuGraphite(gpu_feature_info_, gpu_preferences_);
  }

  default_offscreen_surface_ =
      gl::init::CreateOffscreenGLSurface(gl_display, gfx::Size());

  UMA_HISTOGRAM_ENUMERATION("GPU.GLImplementation", gl::GetGLImplementation());
  InitializeDawnProcs();
}
#else
void GpuInit::InitializeInProcess(base::CommandLine* command_line,
                                  const GpuPreferences& gpu_preferences) {
  gpu_preferences_ = gpu_preferences;
  init_successful_ = true;
#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  params.handle_overlays_swap_failure =
      base::FeatureList::IsEnabled(features::kHandleOverlaysSwapFailure);

  // Page flip testing will only happen in ash-chrome, not in lacros-chrome.
  // Therefore, we only allow or disallow sync and real buffer page flip
  // testing for ash-chrome.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  params.allow_sync_and_real_buffer_page_flip_testing = true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ui::OzonePlatform::InitializeForGPU(params);
#endif
  bool needs_more_info = true;
#if !BUILDFLAG(IS_CASTOS) && !BUILDFLAG(IS_CAST_ANDROID)
  needs_more_info = false;
  CollectBasicGraphicsInfo(command_line, &gpu_info_);
#if defined(SUBPIXEL_FONT_RENDERING_DISABLED)
  gpu_info_.subpixel_font_rendering = false;
#else
  gpu_info_.subpixel_font_rendering = true;
#endif
  gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                            command_line, &needs_more_info);
  if (SwitchableGPUsSupported(gpu_info_, *command_line)) {
    InitializeSwitchableGPUs(
        gpu_feature_info_.enabled_gpu_driver_bug_workarounds);
  }
#endif  // !BUILDFLAG(IS_CASTOS) && !BUILDFLAG(IS_CAST_ANDROID)

  gl::GLDisplay* gl_display = nullptr;

  gl_use_swiftshader_ = EnableSwiftShaderIfNeeded(
      command_line, gpu_feature_info_,
      gpu_preferences_.disable_software_rasterizer, needs_more_info);
  gl_display = gl::init::InitializeGLNoExtensionsOneOff(
      /*init_bindings=*/true,
      /*gpu_preference=*/gl::GpuPreference::kDefault);
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
          /*init_bindings=*/true,
          /*gpu_preference=*/gl::GpuPreference::kDefault);
      if (!gl_display) {
        VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed "
                << "with SwiftShader";
        return;
      }
    }
  }

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

  if (!gl_disabled) {
    if (!CollectGpuExtraInfo(&gpu_extra_info_, gpu_preferences)) {
      VLOG(1) << "gpu::CollectGpuExtraInfo failed";
    }
  }

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
          /*init_bindings=*/true,
          /*gpu_preference=*/gl::GpuPreference::kDefault);
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
  const std::vector<gfx::BufferFormat>
      supported_buffer_formats_for_gl_native_pixmap_import =
          ui::OzonePlatform::GetInstance()
              ->GetSurfaceFactoryOzone()
              ->GetSupportedFormatsForGLNativePixmapImport();
  gpu_feature_info_.supported_buffer_formats_for_allocation_and_texturing =
      std::move(supported_buffer_formats_for_texturing);
  gpu_feature_info_.supported_buffer_formats_for_gl_native_pixmap_import =
      std::move(supported_buffer_formats_for_gl_native_pixmap_import);
#endif

  DisableInProcessGpuVulkan(&gpu_feature_info_, &gpu_preferences_);

  UMA_HISTOGRAM_ENUMERATION("GPU.GLImplementation", gl::GetGLImplementation());

  InitializeDawnProcs();
  if (gpu_preferences_.gr_context_type == GrContextType::kGraphiteDawn) {
    if (!InitializeDawn()) {
      if (gpu_feature_info_.status_values[GPU_FEATURE_TYPE_SKIA_GRAPHITE] !=
          kGpuFeatureStatusEnabled) {
        // SkiaGraphite is disabled by software_rendering_list.json
        gpu_preferences_.gr_context_type = GrContextType::kGL;
      } else {
        LOG(FATAL) << "InitializeDawn() failed!";
      }
    }
  }
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

bool GpuInit::InitializeDawn() {
#if BUILDFLAG(SKIA_USE_DAWN)
  if (gpu_feature_info_.status_values[GPU_FEATURE_TYPE_SKIA_GRAPHITE] !=
          kGpuFeatureStatusEnabled &&
      !gpu::DawnContextProvider::DefaultForceFallbackAdapter()) {
    // Return false, if skia_graphite is blocked in
    // gpu/config/software_rendering_list.json. Unless dawn is using the
    // fallback adaptor (SwiftShader) for testing.
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  auto validate_adapter_fn = [this](wgpu::BackendType backend_type,
                                    wgpu::Adapter adapter) {
    if (backend_type == wgpu::BackendType::Vulkan) {
      // Check if the GPU and driver version are suitable for using Vulkan
      // based hardware acceleration.
      wgpu::AdapterInfo adapter_info;
      wgpu::AdapterPropertiesVk adapter_properties_vk;
      adapter_info.nextInChain = &adapter_properties_vk;
      adapter.GetInfo(&adapter_info);

      VulkanPhysicalDeviceProperties device_properties;
      device_properties.device_name = adapter_info.device;
      device_properties.vendor_id = adapter_info.vendorID;
      device_properties.device_id = adapter_info.deviceID;
      device_properties.driver_version = adapter_properties_vk.driverVersion;

      if (!CheckVulkanCompatibilities(device_properties, gpu_info_)) {
        return false;
      }

      gpu_info_.hardware_supports_vulkan = true;

      // Limit the use of Vulkan's vendorID and deviceID to Android. This is
      // because other platforms, for example, Linux, collect such information
      // somewhere else and we don't want to overwrite it.
      gpu_info_.gpu.vendor_id = device_properties.vendor_id;
      gpu_info_.gpu.device_id = device_properties.device_id;
    }
    return true;
  };
#else
  auto validate_adapter_fn = DawnContextProvider::DefaultValidateAdapterFn;
#endif

  dawn_context_provider_ = gpu::DawnContextProvider::Create(
      gpu_preferences_, validate_adapter_fn,
      GpuDriverBugWorkarounds(
          gpu_feature_info_.enabled_gpu_driver_bug_workarounds));
  if (dawn_context_provider_) {
    return true;
  }
#endif

  LOG(ERROR) << "Failed to create Dawn context provider for Graphite";
  return false;
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

  if (!use_swiftshader && !forced_native) {
    // This can be used as a finch kill switch in case Vulkan is accidentally
    // enabled on a device that it doesn't work properly with.
    const base::FeatureParam<std::string> disable_by_renderer(
        &features::kVulkan, "disable_by_gl_renderer", "");
    if (MatchGLInfo(gpu_info_.gl_renderer, disable_by_renderer.Get())) {
      return false;
    }
  }

  vulkan_implementation_ = CreateVulkanImplementation(
      vulkan_use_swiftshader, gpu_preferences_.enable_vulkan_protected_memory);
  if (!vulkan_implementation_ ||
      !vulkan_implementation_->InitializeVulkanInstance(
          !gpu_preferences_.disable_vulkan_surface)) {
    LOG(ERROR) << "Failed to create and initialize Vulkan implementation.";
    vulkan_implementation_ = nullptr;
    CHECK(!gpu_preferences_.disable_vulkan_fallback_to_gl_for_testing);
  }

  // Vulkan info is no longer collected in gpu/config/gpu_info_collector_win.cc
  // Histogram GPU.SupportsVulkan and GPU.VulkanVersion were marked as expired.
  // TODO(magchen): Add back these two histograms here and re-enable them in
  // histograms.xml when we start Vulkan finch on Windows.

  if (!vulkan_implementation_) {
    return false;
  }

  auto& vulkan_info =
      vulkan_implementation_->GetVulkanInstance()->vulkan_info();
  if (vulkan_info.physical_devices.empty()) {
    return false;
  }

  VulkanPhysicalDeviceProperties device_properties(
      vulkan_info.physical_devices.front().properties);
  if (!use_swiftshader && !forced_native &&
      !CheckVulkanCompatibilities(device_properties, gpu_info_)) {
    vulkan_implementation_.reset();
    return false;
  }

  gpu_info_.hardware_supports_vulkan = true;
  gpu_info_.vulkan_info =
      vulkan_implementation_->GetVulkanInstance()->vulkan_info();
  // Limit the use of Vulkan's vendorID and deviceID to Android.
  // This is because other platforms, for example, Linux, collect such
  // information somewhere else and we don't want to overwrite it.
#if BUILDFLAG(IS_ANDROID)
  gpu_info_.gpu.vendor_id = device_properties.vendor_id;
  gpu_info_.gpu.device_id = device_properties.device_id;
#endif  // BUILDFLAG(IS_ANDROID)

  return true;
#else   // !BUILDFLAG(ENABLE_VULKAN)
  return false;
#endif  // BUILDFLAG(ENABLE_VULKAN)
}

}  // namespace gpu

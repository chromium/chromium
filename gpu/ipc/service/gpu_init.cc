// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_init.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_switching.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_WIN)
#include "gpu/ipc/service/direct_composition_surface_win.h"
#include "ui/gl/gl_surface_egl.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/android_image_reader_compat.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

namespace gpu {

namespace {
bool CollectGraphicsInfo(GPUInfo* gpu_info,
                         const GpuPreferences& gpu_preferences) {
  DCHECK(gpu_info);
  TRACE_EVENT0("gpu,startup", "Collect Graphics Info");
  base::TimeTicks before_collect_context_graphics_info = base::TimeTicks::Now();
  bool success = CollectContextGraphicsInfo(gpu_info, gpu_preferences);
  if (!success)
    LOG(ERROR) << "gpu::CollectGraphicsInfo failed.";

#if defined(OS_WIN)
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLGLES2) {
    gpu_info->direct_composition_overlays =
        DirectCompositionSurfaceWin::AreOverlaysSupported();
    gpu_info->overlay_capabilities =
        DirectCompositionSurfaceWin::GetOverlayCapabilities();
  }
#endif  // defined(OS_WIN)

  if (success) {
    base::TimeDelta collect_context_time =
        base::TimeTicks::Now() - before_collect_context_graphics_info;
    UMA_HISTOGRAM_TIMES("GPU.CollectContextGraphicsInfo", collect_context_time);
  }
  return success;
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(IS_CHROMECAST)
bool CanAccessNvidiaDeviceFile() {
  bool res = true;
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::WILL_BLOCK);
  if (access("/dev/nvidiactl", R_OK) != 0) {
    DVLOG(1) << "NVIDIA device file /dev/nvidiactl access denied";
    res = false;
  }
  return res;
}
#endif  // OS_LINUX && !OS_CHROMEOS && !IS_CHROMECAST

}  // namespace

GpuInit::GpuInit() = default;

GpuInit::~GpuInit() {
  gpu::StopForceDiscreteGPU();
}

bool GpuInit::InitializeAndStartSandbox(base::CommandLine* command_line,
                                        const GpuPreferences& gpu_preferences) {
  gpu_preferences_ = gpu_preferences;
  // Blacklist decisions based on basic GPUInfo may not be final. It might
  // need more context based GPUInfo. In such situations, switching to
  // SwiftShader needs to wait until creating a context.
  bool needs_more_info = true;
#if !defined(OS_ANDROID) && !defined(IS_CHROMECAST)
  needs_more_info = false;
  if (!PopGPUInfoCache(&gpu_info_)) {
    CollectBasicGraphicsInfo(command_line, &gpu_info_);
  }
#if defined(OS_WIN)
  GpuSeriesType gpu_series_type = GetGpuSeriesType(
      gpu_info_.active_gpu().vendor_id, gpu_info_.active_gpu().device_id);
  UMA_HISTOGRAM_ENUMERATION("GPU.GpuGeneration", gpu_series_type);
#endif  // OS_WIN

  // Set keys for crash logging based on preliminary gpu info, in case we
  // crash during feature collection.
  gpu::SetKeysForCrashLogging(gpu_info_);

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (gpu_info_.gpu.vendor_id == 0x10de &&  // NVIDIA
      gpu_info_.gpu.driver_vendor == "NVIDIA" && !CanAccessNvidiaDeviceFile())
    return false;
#endif
  if (!PopGpuFeatureInfoCache(&gpu_feature_info_)) {
    // Compute blacklist and driver bug workaround decisions based on basic GPU
    // info.
    gpu_feature_info_ = gpu::ComputeGpuFeatureInfo(
        gpu_info_, gpu_preferences_, command_line, &needs_more_info);
  }
#endif  // !OS_ANDROID && !IS_CHROMECAST
  gpu_info_.in_process_gpu = false;

  bool use_swiftshader = false;

  // GL bindings may have already been initialized, specifically on MacOSX.
  bool gl_initialized = gl::GetGLImplementation() != gl::kGLImplementationNone;
  if (!gl_initialized) {
    // If GL has already been initialized, then it's too late to select GPU.
    if (gpu::SwitchableGPUsSupported(gpu_info_, *command_line)) {
      gpu::InitializeSwitchableGPUs(
          gpu_feature_info_.enabled_gpu_driver_bug_workarounds);
    }
  } else if (gl::GetGLImplementation() == gl::kGLImplementationSwiftShaderGL &&
             command_line->GetSwitchValueASCII(switches::kUseGL) !=
                 gl::kGLImplementationSwiftShaderName) {
    use_swiftshader = true;
  }

  bool enable_watchdog = !gpu_preferences_.disable_gpu_watchdog &&
                         !command_line->HasSwitch(switches::kHeadless);

  // Disable the watchdog in debug builds because they tend to only be run by
  // developers who will not appreciate the watchdog killing the GPU process.
#ifndef NDEBUG
  enable_watchdog = false;
#endif

  bool delayed_watchdog_enable = false;

#if defined(OS_CHROMEOS)
  // Don't start watchdog immediately, to allow developers to switch to VT2 on
  // startup.
  delayed_watchdog_enable = true;
#endif

  // Start the GPU watchdog only after anything that is expected to be time
  // consuming has completed, otherwise the process is liable to be aborted.
  if (enable_watchdog && !delayed_watchdog_enable) {
    watchdog_thread_ = gpu::GpuWatchdogThread::Create(
        gpu_preferences_.watchdog_starts_backgrounded);
#if defined(OS_WIN)
    // This is a workaround for an occasional deadlock between watchdog and
    // current thread. Watchdog hangs at thread initialization in
    // __acrt_thread_attach() and current thread in std::setlocale(...)
    // (during InitializeGLOneOff()). Source of the deadlock looks like an old
    // UCRT bug that was supposed to be fixed in 10.0.10586 release of UCRT,
    // but we might have come accross a not-yet-covered scenario.
    // References:
    // https://bugs.python.org/issue26624
    // http://stackoverflow.com/questions/35572792/setlocale-stuck-on-windows
    auto watchdog_started = watchdog_thread_->WaitUntilThreadStarted();
    DCHECK(watchdog_started);
#endif  // OS_WIN
  }

  sandbox_helper_->PreSandboxStartup();

  bool attempted_startsandbox = false;
#if defined(OS_LINUX)
  // On Chrome OS ARM Mali, GPU driver userspace creates threads when
  // initializing a GL context, so start the sandbox early.
  // TODO(zmo): Need to collect OS version before this.
  if (gpu_preferences_.gpu_sandbox_start_early) {
    gpu_info_.sandboxed = sandbox_helper_->EnsureSandboxInitialized(
        watchdog_thread_.get(), &gpu_info_, gpu_preferences_);
    attempted_startsandbox = true;
  }
#endif  // defined(OS_LINUX)

  base::TimeTicks before_initialize_one_off = base::TimeTicks::Now();

#if defined(USE_OZONE)
  // Initialize Ozone GPU after the watchdog in case it hangs. The sandbox
  // may also have started at this point.
  ui::OzonePlatform::InitParams params;
  params.single_process = false;
  params.using_mojo =
      features::IsOzoneDrmMojo() || ui::OzonePlatform::EnsureInstance()
                                        ->GetPlatformProperties()
                                        .requires_mojo;
  ui::OzonePlatform::InitializeForGPU(params);
#endif

#if BUILDFLAG(ENABLE_VULKAN)
  if (gpu_preferences_.enable_vulkan) {
    vulkan_implementation_ = gpu::CreateVulkanImplementation();
    if (!vulkan_implementation_ ||
        !vulkan_implementation_->InitializeVulkanInstance()) {
      DLOG(WARNING) << "Failed to create and initialize Vulkan implementation.";
      vulkan_implementation_ = nullptr;
    }
    gpu_preferences_.enable_vulkan = !!vulkan_implementation_;
  }
#else
  gpu_preferences_.enable_vulkan = false;
#endif

  if (!use_swiftshader) {
    use_swiftshader = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, needs_more_info);
  }
  if (gl_initialized && use_swiftshader &&
      gl::GetGLImplementation() != gl::kGLImplementationSwiftShaderGL) {
    gl::init::ShutdownGL(true);
    gl_initialized = false;
  }
  if (!gl_initialized)
    gl_initialized = gl::init::InitializeGLNoExtensionsOneOff();
  if (!gl_initialized) {
    VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed";
    return false;
  }
  bool gl_disabled = gl::GetGLImplementation() == gl::kGLImplementationDisabled;

  // We need to collect GL strings (VENDOR, RENDERER) for blacklisting purposes.
  if (!gl_disabled && !use_swiftshader) {
    if (!CollectGraphicsInfo(&gpu_info_, gpu_preferences_))
      return false;
    gpu::SetKeysForCrashLogging(gpu_info_);
    gpu_feature_info_ = gpu::ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                                   command_line, nullptr);
    use_swiftshader = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, false);
    if (use_swiftshader) {
      gl::init::ShutdownGL(true);
      if (!gl::init::InitializeGLNoExtensionsOneOff()) {
        VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff with SwiftShader "
                << "failed";
        return false;
      }
    }
  }

  if (!gl_disabled) {
    if (!gpu_feature_info_.disabled_extensions.empty()) {
      gl::init::SetDisabledExtensionsPlatform(
          gpu_feature_info_.disabled_extensions);
    }
    if (!gl::init::InitializeExtensionSettingsOneOffPlatform()) {
      VLOG(1) << "gl::init::InitializeExtensionSettingsOneOffPlatform failed";
      return false;
    }
    default_offscreen_surface_ =
        gl::init::CreateOffscreenGLSurface(gfx::Size());
    if (!default_offscreen_surface_) {
      VLOG(1) << "gl::init::CreateOffscreenGLSurface failed";
      return false;
    }
  }

#if defined(OS_LINUX)
  // Driver may create a compatibility profile context when collect graphics
  // information on Linux platform. Try to collect graphics information
  // based on core profile context after disabling platform extensions.
  if (!gl_disabled && !use_swiftshader) {
    if (!CollectGraphicsInfo(&gpu_info_, gpu_preferences_))
      return false;
    gpu::SetKeysForCrashLogging(gpu_info_);
    gpu_feature_info_ = gpu::ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                                   command_line, nullptr);
    use_swiftshader = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, false);
    if (use_swiftshader) {
      gl::init::ShutdownGL(true);
      if (!gl::init::InitializeGLNoExtensionsOneOff()) {
        VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff with SwiftShader "
                << "failed";
        return false;
      }
    }
  }
#endif  // defined(OS_LINUX)

  if (use_swiftshader) {
    AdjustInfoToSwiftShader();
  }

  if (kGpuFeatureStatusEnabled !=
      gpu_feature_info_
          .status_values[GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE]) {
    gpu_preferences_.disable_accelerated_video_decode = true;
  }

  base::TimeDelta initialize_one_off_time =
      base::TimeTicks::Now() - before_initialize_one_off;
  UMA_HISTOGRAM_MEDIUM_TIMES("GPU.InitializeOneOffMediumTime",
                             initialize_one_off_time);

  // Software GL is expected to run slowly, so disable the watchdog
  // in that case.
  // In SwiftShader case, the implementation is actually EGLGLES2.
  if (!use_swiftshader && command_line->HasSwitch(switches::kUseGL)) {
    std::string use_gl = command_line->GetSwitchValueASCII(switches::kUseGL);
    if (use_gl == gl::kGLImplementationSwiftShaderName ||
        use_gl == gl::kGLImplementationSwiftShaderForWebGLName) {
      use_swiftshader = true;
    }
  }
  if (use_swiftshader ||
      gl::GetGLImplementation() == gl::GetSoftwareGLImplementation()) {
    gpu_info_.software_rendering = true;
    if (watchdog_thread_)
      watchdog_thread_->Stop();
    watchdog_thread_ = nullptr;
  } else if (enable_watchdog && delayed_watchdog_enable) {
    watchdog_thread_ = gpu::GpuWatchdogThread::Create(
        gpu_preferences_.watchdog_starts_backgrounded);
  }

  if (!gpu_info_.sandboxed && !attempted_startsandbox) {
    gpu_info_.sandboxed = sandbox_helper_->EnsureSandboxInitialized(
        watchdog_thread_.get(), &gpu_info_, gpu_preferences_);
  }
  UMA_HISTOGRAM_BOOLEAN("GPU.Sandbox.InitializedSuccessfully",
                        gpu_info_.sandboxed);

  gpu_info_.passthrough_cmd_decoder =
      gles2::UsePassthroughCommandDecoder(command_line) &&
      gles2::PassthroughCommandDecoderSupported();

  init_successful_ = true;
#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()->AfterSandboxEntry();
#endif

#if defined(OS_ANDROID)
  // Disable AImageReader if the workaround is enabled.
  if (gpu_feature_info_.IsWorkaroundEnabled(DISABLE_AIMAGEREADER)) {
    base::android::AndroidImageReader::DisableSupport();
  }
#endif

  return true;
}

#if defined(OS_ANDROID)
void GpuInit::InitializeInProcess(base::CommandLine* command_line,
                                  const GpuPreferences& gpu_preferences) {
  gpu_preferences_ = gpu_preferences;
  init_successful_ = true;
  DCHECK(!EnableSwiftShaderIfNeeded(
      command_line, gpu_feature_info_,
      gpu_preferences_.disable_software_rasterizer, false));

  InitializeGLThreadSafe(command_line, gpu_preferences_, &gpu_info_,
                         &gpu_feature_info_);

  default_offscreen_surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());

  // Disable AImageReader if the workaround is enabled.
  if (gpu_feature_info_.IsWorkaroundEnabled(DISABLE_AIMAGEREADER)) {
    base::android::AndroidImageReader::DisableSupport();
  }
}
#else
void GpuInit::InitializeInProcess(base::CommandLine* command_line,
                                  const GpuPreferences& gpu_preferences) {
  gpu_preferences_ = gpu_preferences;
  init_successful_ = true;
#if defined(USE_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  params.using_mojo =
      features::IsOzoneDrmMojo() || ui::OzonePlatform::EnsureInstance()
                                        ->GetPlatformProperties()
                                        .requires_mojo;
  ui::OzonePlatform::InitializeForGPU(params);
  ui::OzonePlatform::GetInstance()->AfterSandboxEntry();
#endif
  bool needs_more_info = true;
#if !defined(IS_CHROMECAST)
  needs_more_info = false;
  if (!PopGPUInfoCache(&gpu_info_)) {
    CollectBasicGraphicsInfo(command_line, &gpu_info_);
  }
  if (!PopGpuFeatureInfoCache(&gpu_feature_info_)) {
    gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                              command_line, &needs_more_info);
  }
  if (SwitchableGPUsSupported(gpu_info_, *command_line)) {
    InitializeSwitchableGPUs(
        gpu_feature_info_.enabled_gpu_driver_bug_workarounds);
  }
#endif  // !IS_CHROMECAST

  bool use_swiftshader = EnableSwiftShaderIfNeeded(
      command_line, gpu_feature_info_,
      gpu_preferences_.disable_software_rasterizer, needs_more_info);
  if (!gl::init::InitializeGLNoExtensionsOneOff()) {
    VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed";
    return;
  }
  bool gl_disabled = gl::GetGLImplementation() == gl::kGLImplementationDisabled;

  if (!gl_disabled && !use_swiftshader) {
    CollectContextGraphicsInfo(&gpu_info_, gpu_preferences_);
    gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                              command_line, nullptr);
    use_swiftshader = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, false);
    if (use_swiftshader) {
      gl::init::ShutdownGL(true);
      if (!gl::init::InitializeGLNoExtensionsOneOff()) {
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
    if (!gl::init::InitializeExtensionSettingsOneOffPlatform()) {
      VLOG(1) << "gl::init::InitializeExtensionSettingsOneOffPlatform failed";
    }
    default_offscreen_surface_ =
        gl::init::CreateOffscreenGLSurface(gfx::Size());
    if (!default_offscreen_surface_) {
      VLOG(1) << "gl::init::CreateOffscreenGLSurface failed";
    }
  }

#if defined(OS_LINUX)
  // Driver may create a compatibility profile context when collect graphics
  // information on Linux platform. Try to collect graphics information
  // based on core profile context after disabling platform extensions.
  if (!gl_disabled && !use_swiftshader) {
    CollectContextGraphicsInfo(&gpu_info_, gpu_preferences_);
    gpu_feature_info_ = ComputeGpuFeatureInfo(gpu_info_, gpu_preferences_,
                                              command_line, nullptr);
    use_swiftshader = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, false);
    if (use_swiftshader) {
      gl::init::ShutdownGL(true);
      if (!gl::init::InitializeGLNoExtensionsOneOff()) {
        VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed "
                << "with SwiftShader";
        return;
      }
    }
  }
#endif  // defined(OS_LINUX)

  if (use_swiftshader) {
    AdjustInfoToSwiftShader();
  }
}
#endif  // OS_ANDROID

void GpuInit::AdjustInfoToSwiftShader() {
  gpu_info_for_hardware_gpu_ = gpu_info_;
  gpu_feature_info_for_hardware_gpu_ = gpu_feature_info_;
  gpu_feature_info_ = ComputeGpuFeatureInfoForSwiftShader();
  CollectContextGraphicsInfo(&gpu_info_, gpu_preferences_);
}

}  // namespace gpu

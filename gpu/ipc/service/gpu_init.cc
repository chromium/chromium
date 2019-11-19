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
#include "components/viz/common/features.h"
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
#include "gpu/ipc/service/gpu_watchdog_thread_v2.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/switches.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

#if defined(OS_WIN)
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_surface_egl.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/android_image_reader_compat.h"
#include "ui/gl/android/android_surface_control_compat.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"
#endif

namespace gpu {

namespace {
bool CollectGraphicsInfo(GPUInfo* gpu_info,
                         const GpuPreferences& gpu_preferences) {
  DCHECK(gpu_info);
  TRACE_EVENT0("gpu,startup", "Collect Graphics Info");
  base::TimeTicks before_collect_context_graphics_info = base::TimeTicks::Now();
  bool success = CollectContextGraphicsInfo(gpu_info);
  if (!success)
    LOG(ERROR) << "gpu::CollectGraphicsInfo failed.";

  if (success) {
    base::TimeDelta collect_context_time =
        base::TimeTicks::Now() - before_collect_context_graphics_info;
    UMA_HISTOGRAM_TIMES("GPU.CollectContextGraphicsInfo", collect_context_time);
  }
  return success;
}

#if defined(OS_WIN)
OverlaySupport FlagsToOverlaySupport(UINT flags) {
  if (flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING)
    return OverlaySupport::kScaling;
  if (flags & DXGI_OVERLAY_SUPPORT_FLAG_DIRECT)
    return OverlaySupport::kDirect;
  return OverlaySupport::kNone;
}
#endif  // OS_WIN

void InitializePlatformOverlaySettings(GPUInfo* gpu_info) {
#if defined(OS_WIN)
  // This has to be called after a context is created, active GPU is identified,
  // and GPU driver bug workarounds are computed again. Otherwise the workaround
  // |disable_direct_composition| may not be correctly applied.
  // Also, this has to be called after falling back to SwiftShader decision is
  // finalized because this function depends on GL is ANGLE's GLES or not.
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE) {
    DCHECK(gpu_info);
    gpu_info->direct_composition =
        gl::DirectCompositionSurfaceWin::IsDirectCompositionSupported();
    gpu_info->supports_overlays =
        gl::DirectCompositionSurfaceWin::AreOverlaysSupported();
    gpu_info->nv12_overlay_support = FlagsToOverlaySupport(
        gl::DirectCompositionSurfaceWin::GetOverlaySupportFlags(
            DXGI_FORMAT_NV12));
    gpu_info->yuy2_overlay_support = FlagsToOverlaySupport(
        gl::DirectCompositionSurfaceWin::GetOverlaySupportFlags(
            DXGI_FORMAT_YUY2));
  }
#elif defined(OS_ANDROID)
  if (gpu_info->gpu.vendor_string == "Qualcomm")
    gl::SurfaceControl::EnableQualcommUBWC();
#endif
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(IS_CHROMECAST)
bool CanAccessNvidiaDeviceFile() {
  bool res = true;
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  if (access("/dev/nvidiactl", R_OK) != 0) {
    DVLOG(1) << "NVIDIA device file /dev/nvidiactl access denied";
    res = false;
  }
  return res;
}
#endif  // OS_LINUX && !OS_CHROMEOS && !IS_CHROMECAST

class GpuWatchdogInit {
 public:
  GpuWatchdogInit() = default;
  ~GpuWatchdogInit() {
    if (watchdog_ptr_)
      watchdog_ptr_->OnInitComplete();
  }

  void SetGpuWatchdogPtr(gpu::GpuWatchdogThread* ptr) { watchdog_ptr_ = ptr; }

 private:
  gpu::GpuWatchdogThread* watchdog_ptr_ = nullptr;
};
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
#if defined(SUBPIXEL_FONT_RENDERING_DISABLED)
  gpu_info_.subpixel_font_rendering = false;
#else
  gpu_info_.subpixel_font_rendering = true;
#endif

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

  // watchdog_init will call watchdog OnInitComplete() at the end of this
  // function.
  GpuWatchdogInit watchdog_init;

  bool delayed_watchdog_enable = false;

#if defined(OS_CHROMEOS)
  // Don't start watchdog immediately, to allow developers to switch to VT2 on
  // startup.
  delayed_watchdog_enable = true;
#endif

  // Start the GPU watchdog only after anything that is expected to be time
  // consuming has completed, otherwise the process is liable to be aborted.
  if (enable_watchdog && !delayed_watchdog_enable) {
    if (base::FeatureList::IsEnabled(features::kGpuWatchdogV2)) {
      watchdog_thread_ = gpu::GpuWatchdogThreadImplV2::Create(
          gpu_preferences_.watchdog_starts_backgrounded);
      watchdog_init.SetGpuWatchdogPtr(watchdog_thread_.get());
    } else {
      watchdog_thread_ = gpu::GpuWatchdogThreadImplV1::Create(
          gpu_preferences_.watchdog_starts_backgrounded);
    }

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
  params.using_mojo = features::IsOzoneDrmMojo();
  params.viz_display_compositor = features::IsVizDisplayCompositorEnabled();
  ui::OzonePlatform::InitializeForGPU(params);
  const std::vector<gfx::BufferFormat> supported_buffer_formats_for_texturing =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->GetSupportedFormatsForTexturing();
#endif

  if (!use_swiftshader) {
    use_swiftshader = EnableSwiftShaderIfNeeded(
        command_line, gpu_feature_info_,
        gpu_preferences_.disable_software_rasterizer, needs_more_info);
  }
  if (gl_initialized && use_swiftshader &&
      gl::GetGLImplementation() != gl::kGLImplementationSwiftShaderGL) {
#if defined(OS_LINUX)
    VLOG(1) << "Quit GPU process launch to fallback to SwiftShader cleanly "
            << "on Linux";
    return false;
#else
    gl::init::ShutdownGL(true);
    gl_initialized = false;
#endif  // OS_LINUX
  }
  if (!gl_initialized)
    gl_initialized = gl::init::InitializeGLNoExtensionsOneOff();
  if (!gl_initialized) {
    VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed";
    return false;
  }
  bool gl_disabled = gl::GetGLImplementation() == gl::kGLImplementationDisabled;

  // Compute passthrough decoder status before ComputeGpuFeatureInfo below.
  gpu_info_.passthrough_cmd_decoder =
      gles2::UsePassthroughCommandDecoder(command_line) &&
      gles2::PassthroughCommandDecoderSupported();

  // We need to collect GL strings (VENDOR, RENDERER) for blacklisting purposes.
  if (!gl_disabled) {
    if (!use_swiftshader) {
      if (!CollectGraphicsInfo(&gpu_info_, gpu_preferences_))
        return false;
      gpu::SetKeysForCrashLogging(gpu_info_);
      gpu_feature_info_ = gpu::ComputeGpuFeatureInfo(
          gpu_info_, gpu_preferences_, command_line, nullptr);
      use_swiftshader = EnableSwiftShaderIfNeeded(
          command_line, gpu_feature_info_,
          gpu_preferences_.disable_software_rasterizer, false);
      if (use_swiftshader) {
#if defined(OS_LINUX)
        VLOG(1) << "Quit GPU process launch to fallback to SwiftShader cleanly "
                << "on Linux";
        return false;
#else
        gl::init::ShutdownGL(true);
        if (!gl::init::InitializeGLNoExtensionsOneOff()) {
          VLOG(1)
              << "gl::init::InitializeGLNoExtensionsOneOff with SwiftShader "
              << "failed";
          return false;
        }
#endif  // OS_LINUX
      }
    } else {  // use_swiftshader == true
      switch (gpu_preferences_.use_vulkan) {
        case gpu::VulkanImplementationName::kNative: {
          // Collect GPU info, so we can use backlist to disable vulkan if it is
          // needed.
          gpu::GPUInfo gpu_info;
          if (!CollectGraphicsInfo(&gpu_info, gpu_preferences_))
            return false;
          auto gpu_feature_info = gpu::ComputeGpuFeatureInfo(
              gpu_info, gpu_preferences_, command_line, nullptr);
          gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_VULKAN] =
              gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_VULKAN];
          break;
        }
        case gpu::VulkanImplementationName::kForcedNative:
        case gpu::VulkanImplementationName::kSwiftshader:
          gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_VULKAN] =
              gpu::kGpuFeatureStatusEnabled;
          break;
        case gpu::VulkanImplementationName::kNone:
          gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_VULKAN] =
              gpu::kGpuFeatureStatusDisabled;
          break;
      }
    }
  }

  InitializeVulkan();

  // Collect GPU process info
  if (!gl_disabled) {
    if (!CollectGpuExtraInfo(&gpu_extra_info_))
      return false;
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

  InitializePlatformOverlaySettings(&gpu_info_);

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
      VLOG(1) << "Quit GPU process launch to fallback to SwiftShader cleanly "
              << "on Linux";
      return false;
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
    watchdog_init.SetGpuWatchdogPtr(nullptr);
  } else if (enable_watchdog && delayed_watchdog_enable) {
    if (base::FeatureList::IsEnabled(features::kGpuWatchdogV2)) {
      watchdog_thread_ = gpu::GpuWatchdogThreadImplV2::Create(
          gpu_preferences_.watchdog_starts_backgrounded);
      watchdog_init.SetGpuWatchdogPtr(watchdog_thread_.get());
    } else {
      watchdog_thread_ = gpu::GpuWatchdogThreadImplV1::Create(
          gpu_preferences_.watchdog_starts_backgrounded);
    }
  }

  UMA_HISTOGRAM_ENUMERATION("GPU.GLImplementation", gl::GetGLImplementation());

  if (!gpu_info_.sandboxed && !attempted_startsandbox) {
    gpu_info_.sandboxed = sandbox_helper_->EnsureSandboxInitialized(
        watchdog_thread_.get(), &gpu_info_, gpu_preferences_);
  }
  UMA_HISTOGRAM_BOOLEAN("GPU.Sandbox.InitializedSuccessfully",
                        gpu_info_.sandboxed);

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
#if defined(USE_OZONE)
  gpu_feature_info_.supported_buffer_formats_for_allocation_and_texturing =
      std::move(supported_buffer_formats_for_texturing);
#endif

  if (!watchdog_thread_)
    watchdog_init.SetGpuWatchdogPtr(nullptr);

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
  InitializeVulkan();

  default_offscreen_surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());

  // Disable AImageReader if the workaround is enabled.
  if (gpu_feature_info_.IsWorkaroundEnabled(DISABLE_AIMAGEREADER)) {
    base::android::AndroidImageReader::DisableSupport();
  }

  UMA_HISTOGRAM_ENUMERATION("GPU.GLImplementation", gl::GetGLImplementation());
}
#else
void GpuInit::InitializeInProcess(base::CommandLine* command_line,
                                  const GpuPreferences& gpu_preferences) {
  gpu_preferences_ = gpu_preferences;
  init_successful_ = true;
#if defined(USE_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  params.using_mojo = features::IsOzoneDrmMojo();
  params.viz_display_compositor = features::IsVizDisplayCompositorEnabled();
  ui::OzonePlatform::InitializeForGPU(params);
  const std::vector<gfx::BufferFormat> supported_buffer_formats_for_texturing =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->GetSupportedFormatsForTexturing();
#endif
  bool needs_more_info = true;
#if !defined(IS_CHROMECAST)
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
    CollectContextGraphicsInfo(&gpu_info_);
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

  InitializePlatformOverlaySettings(&gpu_info_);

#if defined(OS_LINUX)
  // Driver may create a compatibility profile context when collect graphics
  // information on Linux platform. Try to collect graphics information
  // based on core profile context after disabling platform extensions.
  if (!gl_disabled && !use_swiftshader) {
    CollectContextGraphicsInfo(&gpu_info_);
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

#if defined(USE_OZONE)
  gpu_feature_info_.supported_buffer_formats_for_allocation_and_texturing =
      std::move(supported_buffer_formats_for_texturing);
#endif

  UMA_HISTOGRAM_ENUMERATION("GPU.GLImplementation", gl::GetGLImplementation());
}
#endif  // OS_ANDROID

void GpuInit::AdjustInfoToSwiftShader() {
  gpu_info_for_hardware_gpu_ = gpu_info_;
  gpu_info_.passthrough_cmd_decoder = false;
  gpu_feature_info_for_hardware_gpu_ = gpu_feature_info_;
  gpu_feature_info_ = ComputeGpuFeatureInfoForSwiftShader();
  CollectContextGraphicsInfo(&gpu_info_);
}

scoped_refptr<gl::GLSurface> GpuInit::TakeDefaultOffscreenSurface() {
  return std::move(default_offscreen_surface_);
}

void GpuInit::InitializeVulkan() {
#if BUILDFLAG(ENABLE_VULKAN)
  if (gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_VULKAN] ==
      gpu::kGpuFeatureStatusEnabled) {
    DCHECK_NE(gpu_preferences_.use_vulkan,
              gpu::VulkanImplementationName::kNone);
    bool vulkan_use_swiftshader = gpu_preferences_.use_vulkan ==
                                  gpu::VulkanImplementationName::kSwiftshader;
    const bool enforce_protected_memory =
        gpu_preferences_.enforce_vulkan_protected_memory;
    vulkan_implementation_ = gpu::CreateVulkanImplementation(
        vulkan_use_swiftshader,
        enforce_protected_memory ? true : false /* allow_protected_memory */,
        enforce_protected_memory);
    if (!vulkan_implementation_ ||
        !vulkan_implementation_->InitializeVulkanInstance(
            !gpu_preferences_.disable_vulkan_surface)) {
      DLOG(ERROR) << "Failed to create and initialize Vulkan implementation.";
      vulkan_implementation_ = nullptr;
      CHECK(!gpu_preferences_.disable_vulkan_fallback_to_gl_for_testing);
    }
    // TODO(penghuang): Remove GPU.SupportsVulkan and GPU.VulkanVersion from
    // //gpu/config/gpu_info_collector_win.cc when we are finch vulkan on
    // Windows.
    if (!vulkan_use_swiftshader) {
      const bool supports_vulkan = !!vulkan_implementation_;
      UMA_HISTOGRAM_BOOLEAN("GPU.SupportsVulkan", supports_vulkan);
      uint32_t vulkan_version = 0;
      if (supports_vulkan) {
        const auto& vulkan_info =
            vulkan_implementation_->GetVulkanInstance()->vulkan_info();
        vulkan_version = vulkan_info.used_api_version;
      }
      UMA_HISTOGRAM_ENUMERATION(
          "GPU.VulkanVersion", ConvertToHistogramVulkanVersion(vulkan_version));
    }
  }
  if (!vulkan_implementation_) {
    if (gpu_preferences_.gr_context_type == gpu::GrContextType::kVulkan) {
      gpu_preferences_.gr_context_type = gpu::GrContextType::kGL;
    }
    gpu_preferences_.use_vulkan = gpu::VulkanImplementationName::kNone;
    gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_VULKAN] =
        gpu::kGpuFeatureStatusDisabled;
  } else {
    gpu_info_.vulkan_info =
        vulkan_implementation_->GetVulkanInstance()->vulkan_info();
  }
#else
  gpu_preferences_.use_vulkan = gpu::VulkanImplementationName::kNone;
  gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_VULKAN] =
      gpu::kGpuFeatureStatusDisabled;
#endif  // BUILDFLAG(ENABLE_VULKAN)
}

}  // namespace gpu

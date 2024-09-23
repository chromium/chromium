// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/gpu_utils.h"

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "components/viz/common/viz_utils.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "media/gpu/buildflags.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_features.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

void KillGpuProcessImpl(content::GpuProcessHost* host) {
  if (host) {
    host->ForceShutdown();
  }
}

bool GetUintFromSwitch(const base::CommandLine* command_line,
                       const std::string_view& switch_string,
                       uint32_t* value) {
  std::string switch_value(command_line->GetSwitchValueASCII(switch_string));
  return base::StringToUint(switch_value, value);
}

}  // namespace

namespace content {

bool ShouldEnableAndroidSurfaceControl(const base::CommandLine& cmd_line) {
#if !BUILDFLAG(IS_ANDROID)
  return false;
#else
  if (viz::PreferRGB565ResourcesForDisplay())
    return false;
  return features::IsAndroidSurfaceControlEnabled();
#endif
}

const gpu::GpuPreferences GetGpuPreferencesFromCommandLine() {
  DCHECK(base::CommandLine::InitializedForCurrentProcess());
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  gpu::GpuPreferences gpu_preferences =
      gpu::gles2::ParseGpuPreferences(command_line);
  gpu_preferences.disable_accelerated_video_decode =
      command_line->HasSwitch(switches::kDisableAcceleratedVideoDecode);
  gpu_preferences.disable_accelerated_video_encode =
      command_line->HasSwitch(switches::kDisableAcceleratedVideoEncode);
#if BUILDFLAG(IS_WIN)
  gpu_preferences.enable_low_latency_dxva =
      !command_line->HasSwitch(switches::kDisableLowLatencyDxva);
  gpu_preferences.enable_zero_copy_dxgi_video =
      !command_line->HasSwitch(switches::kDisableZeroCopyDxgiVideo);
  gpu_preferences.enable_nv12_dxgi_video =
      !command_line->HasSwitch(switches::kDisableNv12DxgiVideo);
#endif
  gpu_preferences.disable_software_rasterizer =
      command_line->HasSwitch(switches::kDisableSoftwareRasterizer) ||
      !features::IsSwiftShaderAllowed(command_line);
  gpu_preferences.log_gpu_control_list_decisions =
      command_line->HasSwitch(switches::kLogGpuControlListDecisions);
  gpu_preferences.gpu_startup_dialog =
      command_line->HasSwitch(switches::kGpuStartupDialog);
  gpu_preferences.disable_gpu_watchdog =
      command_line->HasSwitch(switches::kDisableGpuWatchdog) ||
      command_line->HasSwitch(switches::kSingleProcess) ||
      command_line->HasSwitch(switches::kInProcessGPU);

  gpu_preferences.gpu_sandbox_start_early =
      command_line->HasSwitch(switches::kGpuSandboxStartEarly);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!gpu_preferences.gpu_sandbox_start_early) {
    const chromeos::BrowserParamsProxy* init_params =
        chromeos::BrowserParamsProxy::Get();
    CHECK(init_params);
    switch (init_params->GpuSandboxStartMode()) {
      case crosapi::mojom::BrowserInitParams::GpuSandboxStartMode::kUnspecified:
        // In practice, this is expected to be reached due to version skewing:
        // when ash-chrome is too old to provide a useful value for
        // BrowserInitParams.gpu_sandbox_start_early. In that case, it's better
        // to start the sandbox early than to crash the GPU process (since that
        // process is started with --gpu-sandbox-failures-fatal=yes).
        // This can also be reached on tests when
        // |init_params|->DisableCrosapiForTesting() is true.
      case crosapi::mojom::BrowserInitParams::GpuSandboxStartMode::kEarly:
        gpu_preferences.gpu_sandbox_start_early = true;
        break;
      case crosapi::mojom::BrowserInitParams::GpuSandboxStartMode::kNormal:
        break;
    }
  }
#endif

  gpu_preferences.enable_vulkan_protected_memory =
      command_line->HasSwitch(switches::kEnableVulkanProtectedMemory);
  gpu_preferences.disable_vulkan_fallback_to_gl_for_testing =
      command_line->HasSwitch(switches::kDisableVulkanFallbackToGLForTesting);

  gpu_preferences.enable_gpu_benchmarking_extension =
      command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking);

  gpu_preferences.enable_android_surface_control =
      ShouldEnableAndroidSurfaceControl(*command_line);

  gpu_preferences.enable_native_gpu_memory_buffers =
      command_line->HasSwitch(switches::kEnableNativeGpuMemoryBuffers);

#if BUILDFLAG(IS_ANDROID)
  gpu_preferences.disable_oopr_debug_crash_dump =
      command_line->HasSwitch(switches::kDisableOoprDebugCrashDump);
#endif

  if (GetUintFromSwitch(command_line, switches::kVulkanHeapMemoryLimitMb,
                        &gpu_preferences.vulkan_heap_memory_limit)) {
    gpu_preferences.vulkan_heap_memory_limit *= 1024 * 1024;
  }
  if (GetUintFromSwitch(command_line, switches::kVulkanSyncCpuMemoryLimitMb,
                        &gpu_preferences.vulkan_sync_cpu_memory_limit)) {
    gpu_preferences.vulkan_sync_cpu_memory_limit *= 1024 * 1024;
  }

  gpu_preferences.force_separate_egl_display_for_webgl_testing =
      command_line->HasSwitch(
          switches::kForceSeparateEGLDisplayForWebGLTesting);

  gpu_preferences.enable_webgpu_experimental_features =
      command_line->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures) ||
      base::FeatureList::IsEnabled(
          blink::features::kWebGPUExperimentalFeatures);

  // Some of these preferences are set or adjusted in
  // GpuDataManagerImplPrivate::AppendGpuCommandLine.
  return gpu_preferences;
}

void KillGpuProcess() {
  GpuProcessHost::CallOnUI(FROM_HERE, GPU_PROCESS_KIND_SANDBOXED,
                           false /* force_create */,
                           base::BindOnce(&KillGpuProcessImpl));
}

gpu::GpuChannelEstablishFactory* GetGpuChannelEstablishFactory() {
  return BrowserMainLoop::GetInstance()->gpu_channel_establish_factory();
}

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
void DumpGpuProfilingData(base::OnceClosure callback) {
  content::GpuProcessHost::CallOnUI(
      FROM_HERE, content::GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
      base::BindOnce(
          [](base::OnceClosure callback, content::GpuProcessHost* host) {
            if (host) {
              host->gpu_service()->WriteClangProfilingProfile(
                  std::move(callback));
            } else {
              LOG(ERROR) << "DumpGpuProfilingData() failed to dump.";
              std::move(callback).Run();
            }
          },
          std::move(callback)));
}
#endif  // BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)

}  // namespace content

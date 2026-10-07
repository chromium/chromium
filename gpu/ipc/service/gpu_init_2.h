// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_INIT_2_H_
#define GPU_IPC_SERVICE_GPU_INIT_2_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/config/device_perf_info.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/gpu_init.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"
#include "ui/gfx/gpu_extra_info.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#endif

namespace base {
class CommandLine;
}

namespace gl {
class GLSurface;
}

namespace gpu {

class VulkanImplementation;

// The GPU initialization path where startup optimizations are being developed.
// Selected by GpuInit::Create() when features::kGpuInitOptimization is
// enabled. See GpuInit for details.
class GPU_IPC_SERVICE_EXPORT GpuInit2 : public GpuInit {
 public:
  GpuInit2();

  GpuInit2(const GpuInit2&) = delete;
  GpuInit2& operator=(const GpuInit2&) = delete;

  ~GpuInit2() override;

  // GpuInit:
  void set_sandbox_helper(GpuSandboxHelper* helper) override;
  bool InitializeAndStartSandbox(
      base::CommandLine* command_line,
      const GpuPreferences& gpu_preferences) override;
  void InitializeInProcess(base::CommandLine* command_line,
                           const GpuPreferences& gpu_preferences) override;
  const GPUInfo& gpu_info() const override;
  const GpuFeatureInfo& gpu_feature_info() const override;
  const gfx::GpuExtraInfo& gpu_extra_info() const override;
  const std::optional<GPUInfo>& gpu_info_for_hardware_gpu() const override;
  const std::optional<GpuFeatureInfo>& gpu_feature_info_for_hardware_gpu()
      const override;
  const std::optional<DevicePerfInfo>& device_perf_info() const override;
  const GpuPreferences& gpu_preferences() const override;
  std::unique_ptr<GpuWatchdogThread> TakeWatchdogThread() override;
#if BUILDFLAG(SKIA_USE_DAWN)
  std::unique_ptr<DawnContextProvider> TakeDawnContextProvider() override;
#endif
  scoped_refptr<gl::GLSurface> TakeDefaultOffscreenSurface() override;
  bool init_successful() const override;
  VulkanImplementation* vulkan_implementation() override;

 private:
  // State shared by the steps of InitializeAndStartSandbox(). It only lives
  // for the duration of that call. Defined in gpu_init_2.cc.
  struct InitState;

  // Runs GpuSandboxHelper::PreSandboxStartup(). Must run exactly once, before
  // the sandbox is started.
  void RunPreSandboxStartup(InitState& state);
  // Starts the sandbox and records the result in `gpu_info_`.
  void StartSandbox(InitState& state);

  bool InitializeDawn();
  bool InitializeVulkan();
  void SetSkiaBackendType();
  void RecordUMA();
  void SaveHardwareGpuInfoAndGpuFeatureInfo();
  void AdjustInfoToSwiftShader();

  // InitializeAndStartSandbox steps helper functions.
  void RecordStartupCrashKeys(InitState& state);
  bool CollectBasicInfoAndComputeFeatures(InitState& state);
  void ConfigureGpuSelection(InitState& state);
  void MaybeFallbackToSwiftShaderEarly(InitState& state);
  void StartWatchdogAndMaybeSandboxEarly(InitState& state);
  bool InitializeGLBindingsAndDisplay(InitState& state);
  void SelectCommandDecoder(InitState& state);

  raw_ptr<GpuSandboxHelper> sandbox_helper_ = nullptr;
  bool gl_use_swiftshader_ = false;
  std::unique_ptr<GpuWatchdogThread> watchdog_thread_;

#if BUILDFLAG(SKIA_USE_DAWN)
  std::unique_ptr<DawnContextProvider> dawn_context_provider_;
#endif

  GPUInfo gpu_info_;
  GpuFeatureInfo gpu_feature_info_;
  GpuPreferences gpu_preferences_;
  scoped_refptr<gl::GLSurface> default_offscreen_surface_;
  bool init_successful_ = false;

  // The following data are collected from hardware GPU and saved before
  // switching to SwiftShader.
  std::optional<GPUInfo> gpu_info_for_hardware_gpu_;
  std::optional<GpuFeatureInfo> gpu_feature_info_for_hardware_gpu_;

  gfx::GpuExtraInfo gpu_extra_info_;

  // The following data are collected by the info collection GPU process.
  std::optional<DevicePerfInfo> device_perf_info_;

#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<VulkanImplementation> vulkan_implementation_;
#endif
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_INIT_2_H_

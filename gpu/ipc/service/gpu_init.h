// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_INIT_H_
#define GPU_IPC_SERVICE_GPU_INIT_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/config/device_perf_info.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
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

class GPU_IPC_SERVICE_EXPORT GpuSandboxHelper {
 public:
  virtual ~GpuSandboxHelper() = default;

  virtual void PreSandboxStartup(const GpuPreferences& gpu_prefs,
                                 const GpuDriverBugWorkarounds& workarounds,
                                 const GPUInfo* gpu_info) = 0;

  virtual bool EnsureSandboxInitialized(GpuWatchdogThread* watchdog_thread,
                                        const GPUInfo* gpu_info,
                                        const GpuPreferences& gpu_prefs) = 0;
};

// Pure interface for GPU process initialization. Exposes the state produced by
// initialization (GPUInfo, GpuFeatureInfo, watchdog, Vulkan/Dawn context, etc.)
// to consumers such as VizMainImpl. GpuInit holds no state of its own; each
// implementation owns its members independently:
//   - GpuInit1: the current, shipping initialization path.
//   - GpuInit2: the path where GPU startup optimizations are being developed.
//     Selected when features::kGpuInitOptimization is enabled.
// Use Create() to obtain the implementation appropriate for the current
// feature configuration.
class GPU_IPC_SERVICE_EXPORT GpuInit {
 public:
  // Returns GpuInit2 if features::kGpuInitOptimization is enabled, otherwise
  // GpuInit1. Requires base::FeatureList to be initialized.
  static std::unique_ptr<GpuInit> Create();

  GpuInit(const GpuInit&) = delete;
  GpuInit& operator=(const GpuInit&) = delete;

  virtual ~GpuInit() = default;

  virtual void set_sandbox_helper(GpuSandboxHelper* helper) = 0;

  // TODO(zmo): Get rid of |command_line| in the following two functions.
  // Pass all bits through GpuPreferences.
  virtual bool InitializeAndStartSandbox(
      base::CommandLine* command_line,
      const GpuPreferences& gpu_preferences) = 0;
  virtual void InitializeInProcess(base::CommandLine* command_line,
                                   const GpuPreferences& gpu_preferences) = 0;

  virtual const GPUInfo& gpu_info() const = 0;
  virtual const GpuFeatureInfo& gpu_feature_info() const = 0;
  virtual const gfx::GpuExtraInfo& gpu_extra_info() const = 0;
  virtual const std::optional<GPUInfo>& gpu_info_for_hardware_gpu() const = 0;
  virtual const std::optional<GpuFeatureInfo>&
  gpu_feature_info_for_hardware_gpu() const = 0;
  virtual const std::optional<DevicePerfInfo>& device_perf_info() const = 0;
  virtual const GpuPreferences& gpu_preferences() const = 0;
  virtual std::unique_ptr<GpuWatchdogThread> TakeWatchdogThread() = 0;
#if BUILDFLAG(SKIA_USE_DAWN)
  virtual std::unique_ptr<DawnContextProvider> TakeDawnContextProvider() = 0;
#endif
  virtual scoped_refptr<gl::GLSurface> TakeDefaultOffscreenSurface() = 0;
  virtual bool init_successful() const = 0;
  virtual VulkanImplementation* vulkan_implementation() = 0;

 protected:
  GpuInit() = default;
};

// The current GPU initialization path. See GpuInit for details.
class GPU_IPC_SERVICE_EXPORT GpuInit1 : public GpuInit {
 public:
  GpuInit1();

  GpuInit1(const GpuInit1&) = delete;
  GpuInit1& operator=(const GpuInit1&) = delete;

  ~GpuInit1() override;

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
  bool InitializeDawn();
  bool InitializeVulkan();
  void SetSkiaBackendType();
  void RecordUMA();
  void SaveHardwareGpuInfoAndGpuFeatureInfo();
  void AdjustInfoToSwiftShader();

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

#endif  // GPU_IPC_SERVICE_GPU_INIT_H_

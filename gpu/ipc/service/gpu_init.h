// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_INIT_H_
#define GPU_IPC_SERVICE_GPU_INIT_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "gpu/config/device_perf_info.h"
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

  virtual void PreSandboxStartup(const GpuPreferences& gpu_prefs) = 0;

  virtual bool EnsureSandboxInitialized(GpuWatchdogThread* watchdog_thread,
                                        const GPUInfo* gpu_info,
                                        const GpuPreferences& gpu_prefs) = 0;
};

class GPU_IPC_SERVICE_EXPORT GpuInit {
 public:
  GpuInit();

  GpuInit(const GpuInit&) = delete;
  GpuInit& operator=(const GpuInit&) = delete;

  ~GpuInit();

  void set_sandbox_helper(GpuSandboxHelper* helper) {
    sandbox_helper_ = helper;
  }

  // TODO(zmo): Get rid of |command_line| in the following two functions.
  // Pass all bits through GpuPreferences.
  bool InitializeAndStartSandbox(base::CommandLine* command_line,
                                 const GpuPreferences& gpu_preferences);
  void InitializeInProcess(base::CommandLine* command_line,
                           const GpuPreferences& gpu_preferences);

  const GPUInfo& gpu_info() const { return gpu_info_; }
  const GpuFeatureInfo& gpu_feature_info() const { return gpu_feature_info_; }
  const gfx::GpuExtraInfo& gpu_extra_info() const { return gpu_extra_info_; }
  const std::optional<GPUInfo>& gpu_info_for_hardware_gpu() const {
    return gpu_info_for_hardware_gpu_;
  }
  const std::optional<GpuFeatureInfo>& gpu_feature_info_for_hardware_gpu()
      const {
    return gpu_feature_info_for_hardware_gpu_;
  }
  const std::optional<DevicePerfInfo>& device_perf_info() const {
    return device_perf_info_;
  }
  const GpuPreferences& gpu_preferences() const { return gpu_preferences_; }
  std::unique_ptr<GpuWatchdogThread> TakeWatchdogThread() {
    return std::move(watchdog_thread_);
  }
#if BUILDFLAG(SKIA_USE_DAWN)
  std::unique_ptr<DawnContextProvider> TakeDawnContextProvider() {
    return std::move(dawn_context_provider_);
  }
#endif
  scoped_refptr<gl::GLSurface> TakeDefaultOffscreenSurface();
  bool init_successful() const { return init_successful_; }
#if BUILDFLAG(ENABLE_VULKAN)
  VulkanImplementation* vulkan_implementation() {
    return vulkan_implementation_.get();
  }
#else
  VulkanImplementation* vulkan_implementation() { return nullptr; }
#endif

 private:
  bool InitializeDawn();
  bool InitializeVulkan();

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

  void SaveHardwareGpuInfoAndGpuFeatureInfo();
  void AdjustInfoToSwiftShader();
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_INIT_H_

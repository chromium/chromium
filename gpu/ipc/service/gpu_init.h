// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_INIT_H_
#define GPU_IPC_SERVICE_GPU_INIT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/config/gpu_extra_info.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "gpu/vulkan/buildflags.h"

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

  virtual void PreSandboxStartup() = 0;

  virtual bool EnsureSandboxInitialized(GpuWatchdogThread* watchdog_thread,
                                        const GPUInfo* gpu_info,
                                        const GpuPreferences& gpu_prefs) = 0;
};

class GPU_IPC_SERVICE_EXPORT GpuInit {
 public:
  GpuInit();
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
  const GpuExtraInfo& gpu_extra_info() const { return gpu_extra_info_; }
  const base::Optional<GPUInfo>& gpu_info_for_hardware_gpu() const {
    return gpu_info_for_hardware_gpu_;
  }
  const base::Optional<GpuFeatureInfo>& gpu_feature_info_for_hardware_gpu()
      const {
    return gpu_feature_info_for_hardware_gpu_;
  }
  const GpuPreferences& gpu_preferences() const { return gpu_preferences_; }
  std::unique_ptr<GpuWatchdogThread> TakeWatchdogThread() {
    return std::move(watchdog_thread_);
  }
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
  void InitializeVulkan();

  GpuSandboxHelper* sandbox_helper_ = nullptr;
  std::unique_ptr<GpuWatchdogThread> watchdog_thread_;
  GPUInfo gpu_info_;
  GpuFeatureInfo gpu_feature_info_;
  GpuPreferences gpu_preferences_;
  scoped_refptr<gl::GLSurface> default_offscreen_surface_;
  bool init_successful_ = false;

  // The following data are collected from hardware GPU and saved before
  // switching to SwiftShader.
  base::Optional<GPUInfo> gpu_info_for_hardware_gpu_;
  base::Optional<GpuFeatureInfo> gpu_feature_info_for_hardware_gpu_;

  GpuExtraInfo gpu_extra_info_;

#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<VulkanImplementation> vulkan_implementation_;
#endif

  void AdjustInfoToSwiftShader();

  DISALLOW_COPY_AND_ASSIGN(GpuInit);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_INIT_H_

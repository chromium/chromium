// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/process/process.h"
#include "content/common/content_export.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_feature_type.h"

namespace base {
class CommandLine;
}

namespace gpu {
struct GPUInfo;
struct VideoMemoryUsageStats;
}

namespace content {
enum GpuProcessKind {
  GPU_PROCESS_KIND_INFO_COLLECTION,  // Unsandboxed, no init GL bindings.
  GPU_PROCESS_KIND_SANDBOXED,
  GPU_PROCESS_KIND_COUNT
};

class GpuDataManagerObserver;

// This class is fully thread-safe.
class GpuDataManager {
 public:
  using VideoMemoryUsageStatsCallback =
      base::OnceCallback<void(const gpu::VideoMemoryUsageStats&)>;

  // Getter for the singleton.
  CONTENT_EXPORT static GpuDataManager* GetInstance();

  CONTENT_EXPORT static bool Initialized();

  virtual gpu::GPUInfo GetGPUInfo() = 0;

  virtual gpu::GpuFeatureStatus GetFeatureStatus(
      gpu::GpuFeatureType feature) = 0;

  // True if GPU is accelerated or running on SwiftShader. It means calling
  // Graphics API in the GPU process is allowed. This indicator might change
  // because we could collect more GPU info or because the GPU blocklist could
  // be updated. If this returns false, some GPU process access, including GPU
  // info collection, should be blocked. This function can be called on any
  // thread. If |reason| is not nullptr and GPU access is blocked, upon return,
  // |reason| contains a description of the reason why GPU access is blocked.
  virtual bool GpuAccessAllowed(std::string* reason) = 0;

  // Check if basic and context GPU info have been collected.
  virtual bool IsEssentialGpuInfoAvailable() = 0;

  // Requests that the GPU process report its current video memory usage stats.
  virtual void RequestVideoMemoryUsageStatsUpdate(
      VideoMemoryUsageStatsCallback callback) = 0;

  // Registers/unregister |observer|.
  virtual void AddObserver(GpuDataManagerObserver* observer) = 0;
  virtual void RemoveObserver(GpuDataManagerObserver* observer) = 0;

  virtual void DisableHardwareAcceleration() = 0;

  // Whether a GPU is in use (as opposed to a software renderer).
  virtual bool HardwareAccelerationEnabled() = 0;

  // Insert switches into gpu process command line: kUseGL, etc.
  virtual void AppendGpuCommandLine(base::CommandLine* command_line,
                                    GpuProcessKind kind) = 0;

  // This is only called by extensions testing.
  virtual void BlocklistWebGLForTesting() = 0;

  // This is only called by chrome feedback tests.
  virtual void SetSkiaGraphiteEnabledForTesting(bool enabled) = 0;

 protected:
  virtual ~GpuDataManager() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_H_

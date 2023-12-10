// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_DELEGATE_H_
#define GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_DELEGATE_H_

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/gpu_vsync_callback.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ui/gfx/gpu_fence_handle.h"

namespace gpu {
struct GpuPreferences;

namespace gles2 {
class FeatureInfo;
}

class GPU_IPC_SERVICE_EXPORT ImageTransportSurfaceDelegate {
 public:
#if BUILDFLAG(IS_WIN)
  // Sends the created child window to the browser process so that it can be
  // parented to the browser process window
  virtual void AddChildWindowToBrowser(gpu::SurfaceHandle child_window) = 0;
#endif

  // Returns the features available for the ContextGroup.
  virtual const gles2::FeatureInfo* GetFeatureInfo() const = 0;

  virtual const GpuPreferences& GetGpuPreferences() const = 0;

  // Callback for GPU vsync signal.  May be called on a different thread.
  virtual viz::GpuVSyncCallback GetGpuVSyncCallback() = 0;

 protected:
  virtual ~ImageTransportSurfaceDelegate() = default;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_DELEGATE_H_

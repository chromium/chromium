// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_DATA_MANAGER_VISUAL_PROXY_OZONE_LINUX_H_
#define CONTENT_BROWSER_GPU_DATA_MANAGER_VISUAL_PROXY_OZONE_LINUX_H_

#include "base/scoped_observation.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "ui/gfx/gpu_extra_info.h"

namespace content {

// Forwards GPUInfo updates to ui::XVisualManager on X11 (will be removed as
// soon as Ozone is default on Linux) or PlatformEGLGLUtility.
class GpuDataManagerVisualProxyOzoneLinux : public GpuDataManagerObserver {
 public:
  explicit GpuDataManagerVisualProxyOzoneLinux(
      GpuDataManagerImpl* gpu_data_manager);
  GpuDataManagerVisualProxyOzoneLinux(
      const GpuDataManagerVisualProxyOzoneLinux&) = delete;
  GpuDataManagerVisualProxyOzoneLinux& operator=(
      const GpuDataManagerVisualProxyOzoneLinux&) = delete;
  ~GpuDataManagerVisualProxyOzoneLinux() override;

  void OnGpuInfoUpdate() override;
  void OnGpuExtraInfoUpdate() override;

 private:
  void OnUpdate();
  bool UpdateVisualsOnGpuInfoChanged(bool software_rendering,
                                     x11::VisualId default_visual_id,
                                     x11::VisualId transparent_visual_id);

  GpuDataManagerImpl* gpu_data_manager_;
  base::ScopedObservation<GpuDataManagerImpl, GpuDataManagerObserver>
      scoped_observer_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_DATA_MANAGER_VISUAL_PROXY_OZONE_LINUX_H_

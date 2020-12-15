// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu_data_manager_visual_proxy_ozone_linux.h"

#include "base/command_line.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/switches.h"

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_gl_egl_utility.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_gl_egl_utility.h"  // nogncheck
#endif

namespace content {

#if defined(USE_X11) || defined(USE_OZONE_PLATFORM_X11)
namespace {

void ShutdownGpuOnIO() {
  // The GPU process sent back bad visuals, which should never happen.
  auto* gpu_process_host =
      GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED, false);
  if (gpu_process_host)
    gpu_process_host->ForceShutdown();
}

}  // namespace
#endif  // defined(USE_X11) || defined(USE_OZONE_PLATFORM_X11)

GpuDataManagerVisualProxyOzoneLinux::GpuDataManagerVisualProxyOzoneLinux(
    GpuDataManagerImpl* gpu_data_manager)
    : gpu_data_manager_(gpu_data_manager) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless))
    scoped_observer_.Observe(gpu_data_manager_);
}

GpuDataManagerVisualProxyOzoneLinux::~GpuDataManagerVisualProxyOzoneLinux() =
    default;

void GpuDataManagerVisualProxyOzoneLinux::OnGpuInfoUpdate() {
  OnUpdate();
}

void GpuDataManagerVisualProxyOzoneLinux::OnGpuExtraInfoUpdate() {
  OnUpdate();
}

void GpuDataManagerVisualProxyOzoneLinux::OnUpdate() {
#if defined(USE_OZONE)
  // Early return for Ozone/non-X11.  Otherwise UpdateVisualsOnGpuInfoChanged()
  // will return false, thus terminating the GPU process and ruining things.
  if (features::IsUsingOzonePlatform() &&
      !ui::OzonePlatform::GetInstance()->GetPlatformGLEGLUtility()) {
    return;
  }
#endif

#if defined(USE_X11) || defined(USE_OZONE_PLATFORM_X11)
  gpu::GPUInfo gpu_info = gpu_data_manager_->GetGPUInfo();
  gfx::GpuExtraInfo gpu_extra_info = gpu_data_manager_->GetGpuExtraInfo();
  if (!UpdateVisualsOnGpuInfoChanged(
          gpu_info.software_rendering ||
              !gpu_data_manager_->GpuAccessAllowed(nullptr),
          gpu_extra_info.system_visual, gpu_extra_info.rgba_visual)) {
    GetIOThreadTaskRunner({})->PostTask(FROM_HERE,
                                        base::BindOnce(&ShutdownGpuOnIO));
  }
#endif
}

bool GpuDataManagerVisualProxyOzoneLinux::UpdateVisualsOnGpuInfoChanged(
    bool software_rendering,
    x11::VisualId default_visual_id,
    x11::VisualId transparent_visual_id) {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform() &&
      ui::OzonePlatform::GetInstance()->GetPlatformGLEGLUtility()) {
    return ui::OzonePlatform::GetInstance()
        ->GetPlatformGLEGLUtility()
        ->UpdateVisualsOnGpuInfoChanged(
            software_rendering, static_cast<uint32_t>(default_visual_id),
            static_cast<uint32_t>(transparent_visual_id));
  }
#endif
#if defined(USE_X11)
  return ui::UpdateVisualsOnGpuInfoChanged(
      software_rendering, default_visual_id, transparent_visual_id);
#endif
  NOTREACHED() << "Only X11 may invoke this!";
  return false;
}

}  // namespace content

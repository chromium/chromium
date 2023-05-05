// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_VSYNC_MAC_H_
#define GPU_IPC_SERVICE_GPU_VSYNC_MAC_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/viz/common/gpu/gpu_vsync_callback.h"
#include "gpu/ipc/service/timer_based_vsync_mac.h"
#include "ui/display/mac/display_link_mac.h"
#include "ui/display/types/display_constants.h"

namespace gpu {

// Each ImageTransportSurfaceOverlayMacEGL creates a GpuVSyncMac,
class GpuVSyncMac {
 public:
  explicit GpuVSyncMac(viz::GpuVSyncCallback vsync_callback);
  ~GpuVSyncMac();

  void SetVSyncDisplayID(int64_t display_id);
  void SetGpuVSyncEnabled(bool enabled);

 private:
  // Register a DisplayLinkMac callback. If it fails, the timer will be used
  // instead.
  void OnDisplayLinkCallback(ui::VSyncParamsMac params);

  void AddGpuVSyncCallback();
  void RemoveGpuVSyncCallback();

  // This is the Viz callback for BeginFrame.
  const viz::GpuVSyncCallback vsync_callback_;

  // The timer works as GpuVsync when CVDisplayLink fails.
  raw_ptr<TimerBasedVsyncMac> timer_based_vsync_mac_;

  // CGDirectDisplayID of the current monitor used for Creating CVDisplayLink.
  int64_t display_id_ = display::kInvalidDisplayId;

  // The default frame rate is 60 Hz (16 ms).
  base::TimeDelta nominal_refresh_period_ = base::Hertz(60);

  // Start GpuVsync. Vis enables it when BeginFrame is needed.
  bool gpu_vsync_enabled_ = false;

  scoped_refptr<ui::DisplayLinkMac> display_link_mac_;

  // CVDisplayLink callback. |vsync_callback_mac_| calls the viz callback runner
  // that will runs on the viz thread.
  std::unique_ptr<ui::VSyncCallbackMac> vsync_callback_mac_;

  base::WeakPtrFactory<GpuVSyncMac> weak_ptr_factory_{this};
};
}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_VSYNC_MAC_H_

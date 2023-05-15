// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_TIMER_BASED_VSYNC_MAC_H_
#define GPU_IPC_SERVICE_TIMER_BASED_VSYNC_MAC_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/viz/common/gpu/gpu_vsync_callback.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace gpu {
// Created on the GPU main thread. There is only one instance in the GPU
// process.
class TimerBasedVsyncMac : public base::PowerSuspendObserver {
 public:
  static TimerBasedVsyncMac* GetInstance();

  TimerBasedVsyncMac(const TimerBasedVsyncMac&) = delete;
  TimerBasedVsyncMac& operator=(const TimerBasedVsyncMac&) = delete;

  // Implementation of base::PowerSuspendObserver
  void OnSuspend() override;
  void OnResume() override;

  // The first and the last function after GpuVSyncThread starts running.
  void Init();
  void CleanUp();

  void AddVSyncTimerCallback(viz::GpuVSyncCallback callback);
  void RemoveVSyncTimerCallback(viz::GpuVSyncCallback callback);

 private:
  friend struct base::DefaultSingletonTraits<TimerBasedVsyncMac>;

  TimerBasedVsyncMac();
  ~TimerBasedVsyncMac() override;

  // The timer tick for vsync callback.
  void OnTimerTick();

  // True when it is in a power suspension mode.
  bool is_suspended_ = false;

  // For delay based timer when VCDisplayLink fails..
  base::RepeatingClosure tick_closure_;

  base::DeadlineTimer timer_;

  // All GpuVSyncMac call
  std::vector<viz::GpuVSyncCallback> timer_callbacks_;

  // The default frame rate is 60 Hz (16 ms).
  const base::TimeDelta nominal_refresh_period_ = base::Hertz(60);

  base::TimeTicks last_target_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  SEQUENCE_CHECKER(vsync_thread_sequence_checker_);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_TIMER_BASED_VSYNC_MAC_H_

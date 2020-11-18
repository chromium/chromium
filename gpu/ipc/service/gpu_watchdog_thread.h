// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_H_
#define GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_H_

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_observer.h"
#include "base/task/task_observer.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_watchdog_timeout.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GpuWatchdogThreadEvent {
  kGpuWatchdogStart,
  kGpuWatchdogKill,
  kGpuWatchdogEnd,
  kMaxValue = kGpuWatchdogEnd,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GpuWatchdogTimeoutEvent {
  // Recorded each time OnWatchdogTimeout() is called.
  kTimeout = 0,
  // Recorded when a GPU main thread is killed for a detected hang.
  kKill = 1,
  // Window only: Recorded when a hang is detected but we allow the GPU main
  // thread to continue until it spent the full
  // thread time doing the work.
  kMoreThreadTime = 2,
  // Windows only: The GPU makes progress after givenmore thread time. The GPU
  // main thread is not killed.
  kProgressAfterMoreThreadTime = 3,
  // Deprecated. A gpu hang is detected but watchdog waits for 60 seconds before
  // taking action.
  // kTimeoutWait = 4,
  // Deprecated. The GPU makes progress within 60 sec in OnWatchdogTimeout().
  // The GPU main thread is not killed.
  // kProgressAfterWait = 5,
  // Just continue if it's not on the TTY of our host X11 server.
  kContinueOnNonHostServerTty = 6,
  // Windows only: After detecting GPU hang and continuing running through
  // OnGpuWatchdogTimeout for the max cycles, the GPU main thread still cannot
  // get the full thread time.
  kLessThanFullThreadTimeAfterCapped = 7,
  // Windows only: The GPU main thread went through the
  // kLessThanFullThreadTimeAfterCapped stage before the process is killed.
  kKillOnLessThreadTime = 8,
  // OnWatchdogTimeout() is called long after the expected time. The GPU is not
  // killed this time because of the slow system.
  kSlowWatchdogThread = 9,
  kMaxValue = kSlowWatchdogThread,
};

#if defined(OS_WIN)
// If the actual time the watched GPU thread spent doing actual work is less
// than the watchdog timeout, the GPU thread can continue running through
// OnGPUWatchdogTimeout for at most 4 times before the gpu thread is killed.
constexpr int kMaxCountOfMoreGpuThreadTimeAllowed = 3;
#endif
constexpr int kMaxExtraCyclesBeforeKill = 0;

// A thread that intermitently sends tasks to a group of watched message loops
// and deliberately crashes if one of them does not respond after a timeout.
class GPU_IPC_SERVICE_EXPORT GpuWatchdogThread : public base::Thread,
                                                 public base::PowerObserver,
                                                 public base::TaskObserver,
                                                 public gl::ProgressReporter {
 public:
  static std::unique_ptr<GpuWatchdogThread> Create(bool start_backgrounded);

  static std::unique_ptr<GpuWatchdogThread> Create(
      bool start_backgrounded,
      base::TimeDelta timeout,
      int init_factor,
      int restart_factor,
      bool test_mode);

  ~GpuWatchdogThread() override;

  // Must be called after a PowerMonitor has been created. Can be called from
  // any thread.
  void AddPowerObserver();

  // Notifies the watchdog when Chrome is backgrounded / foregrounded. Should
  // only be used if Chrome is completely backgrounded and not expected to
  // render (all windows backgrounded and not producing frames).
  void OnBackgrounded();
  void OnForegrounded();

  // The watchdog starts armed to catch startup hangs, and needs to be disarmed
  // once init is complete, before executing tasks.
  void OnInitComplete();

  // Notifies the watchdog when the GPU child process is being destroyed.
  // This function is called directly from
  // viz::GpuServiceImpl::~GpuServiceImpl()
  void OnGpuProcessTearDown();

  // Pause the GPU watchdog to stop the timeout task. If the current heavy task
  // is not running on the GPU driver, the watchdog can be paused to avoid
  // unneeded crash.
  void PauseWatchdog();
  // Continue the watchdog after a pause.
  void ResumeWatchdog();

  // For gpu testing only. Return status for the watchdog tests
  bool IsGpuHangDetectedForTesting();

  void WaitForPowerObserverAddedForTesting();

  // Implements base::Thread.
  void Init() override;
  void CleanUp() override;

  // Implements gl::ProgressReporter.
  void ReportProgress() override;

  // Implements TaskObserver.
  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  // Implements base::PowerObserver.
  void OnSuspend() override;
  void OnResume() override;

 protected:
  GpuWatchdogThread();

 private:
  enum PauseResumeSource {
    kAndroidBackgroundForeground = 0,
    kPowerSuspendResume = 1,
    kGeneralGpuFlow = 2,
  };

  GpuWatchdogThread(base::TimeDelta timeout,
                    int init_factor,
                    int restart_factor,
                    bool test_mode);
  void OnAddPowerObserver();
  void RestartWatchdogTimeoutTask(PauseResumeSource source_of_request);
  void StopWatchdogTimeoutTask(PauseResumeSource source_of_request);
  void UpdateInitializationFlag();
  void Arm();
  void Disarm();
  void InProgress();
  bool IsArmed();
  base::subtle::Atomic32 ReadArmDisarmCounter();
  void OnWatchdogTimeout();
  bool SlowWatchdogThread();
  bool WatchedThreadNeedsMoreThreadTime(bool no_gpu_hang_detected);
#if defined(OS_WIN)
  base::ThreadTicks GetWatchedThreadTime();
#endif

  // Do not change the function name. It is used for [GPU HANG] carsh reports.
  void DeliberatelyTerminateToRecoverFromHang();

  // Records "GPU.WatchdogThread.Event".
  void GpuWatchdogHistogram(GpuWatchdogThreadEvent thread_event);

  // Histogram recorded in OnWatchdogTimeout()
  // Records "GPU.WatchdogThread.Timeout"
  void GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent timeout_event);

#if defined(OS_WIN)
  // The extra thread time the GPU main thread needs to make a progress.
  // Records "GPU.WatchdogThread.ExtraThreadTime".
  void RecordExtraThreadTimeHistogram();
  // The number of users per timeout stay in Chrome after giving extra thread
  // time. Records "GPU.WatchdogThread.ExtraThreadTime.NumOfUsers" and
  // "GPU.WatchdogThread.Timeout".
  void RecordNumOfUsersWaitingWithExtraThreadTimeHistogram(int count);

  // Histograms recorded for WatchedThreadNeedsMoreThreadTime() function.
  void WatchedThreadNeedsMoreThreadTimeHistogram(
      bool no_gpu_hang_detected,
      bool start_of_more_thread_time);
#endif

  // Used for metrics. It's 1 minute after the event.
  bool WithinOneMinFromPowerResumed();
  bool WithinOneMinFromForegrounded();

#if defined(USE_X11)
  void UpdateActiveTTY();
#endif
  // The watchdog continues when it's not on the TTY of our host X11 server.
  bool ContinueOnNonHostX11ServerTty();

  // This counter is only written on the gpu thread, and read on both threads.
  volatile base::subtle::Atomic32 arm_disarm_counter_ = 0;
  // The counter number read in the last OnWatchdogTimeout() on the watchdog
  // thread.
  int32_t last_arm_disarm_counter_ = 0;

  // Timeout on the watchdog thread to check if gpu hangs.
  base::TimeDelta watchdog_timeout_;

  // The one-time watchdog timeout multiplier in the gpu initialization.
  int watchdog_init_factor_;

  // The one-time watchdog timeout multiplier after the watchdog pauses and
  // restarts.
  int watchdog_restart_factor_;

  // The time the gpu watchdog was created.
  base::TimeTicks watchdog_start_timeticks_;

  // The time the last OnSuspend and OnResume was called.
  base::TimeTicks power_suspend_timeticks_;
  base::TimeTicks power_resume_timeticks_;

  // The time the last OnBackgrounded and OnForegrounded was called.
  base::TimeTicks backgrounded_timeticks_;
  base::TimeTicks foregrounded_timeticks_;

  // The time PauseWatchdog and ResumeWatchdog was called.
  base::TimeTicks watchdog_pause_timeticks_;
  base::TimeTicks watchdog_resume_timeticks_;

  // TimeTicks: Tracking the amount of time a task runs. Executing delayed
  //            tasks at the right time.
  // ThreadTicks: Use this timer to (approximately) measure how much time the
  // calling thread spent doing actual work vs. being de-scheduled.

  // The time the last OnWatchdogTimeout() was called.
  base::TimeTicks last_on_watchdog_timeout_timeticks_;

  // The wall-clock time the next OnWatchdogTimeout() will be called.
  base::Time next_on_watchdog_timeout_time_;

#if defined(OS_WIN)
  base::ThreadTicks last_on_watchdog_timeout_thread_ticks_;

  // The difference between the timeout and the actual time the watched thread
  // spent doing actual work.
  base::TimeDelta remaining_watched_thread_ticks_;

  // The Windows thread hanndle of the watched GPU main thread.
  void* watched_thread_handle_ = nullptr;

  // After GPU hang detected, how many times has the GPU thread been allowed to
  // continue due to not enough thread time.
  int count_of_more_gpu_thread_time_allowed_ = 0;

  // The total timeout, up to 60 seconds, the watchdog thread waits for the GPU
  // main thread to get full thread time.
  base::TimeDelta time_in_wait_for_full_thread_time_;

  // After detecting GPU hang and continuing running through
  // OnGpuWatchdogTimeout for the max cycles, the GPU main thread still cannot
  // get the full thread time.
  bool less_than_full_thread_time_after_capped_ = false;
#endif

#if defined(USE_X11)
  FILE* tty_file_ = nullptr;
  int host_tty_ = -1;
  int active_tty_ = -1;
  int last_active_tty_ = -1;
#endif

  // The system has entered the power suspension mode.
  bool in_power_suspension_ = false;

  // The GPU process has started tearing down. Accessed only in the gpu process.
  bool in_gpu_process_teardown_ = false;

  // Chrome is running on the background on Android. Gpu is probably very slow
  // or stalled.
  bool is_backgrounded_ = false;

  // The GPU watchdog is paused. The timeout task is temporarily stopped.
  bool is_paused_ = false;

  // Whether the watchdog thread has been called and added to the power monitor
  // observer.
  bool is_add_power_observer_called_ = false;
  bool is_power_observer_added_ = false;

  // whether GpuWatchdogThreadEvent::kGpuWatchdogStart has been recorded.
  bool is_watchdog_start_histogram_recorded = false;

  // Read/Write by the watchdog thread only after initialized in the
  // constructor.
  bool in_gpu_initialization_ = false;

  // The number of logical processors/cores on the current machine.
  int num_of_processors_ = 0;

  // how many cycles of timeout since we detect a hang.
  int count_of_extra_cycles_ = 0;

  // For the experiment and the debugging purpose
  size_t num_of_timeout_after_power_resume_ = 0;
  size_t num_of_timeout_after_foregrounded_ = 0;
  bool foregrounded_event_ = false;
  bool power_resumed_event_ = false;

  // For gpu testing only.
  const bool is_test_mode_;
  // Set by the watchdog thread and Read by the test thread.
  base::AtomicFlag test_result_timeout_and_gpu_hang_;

  scoped_refptr<base::SingleThreadTaskRunner> watched_gpu_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> watchdog_thread_task_runner_;

  base::WeakPtr<GpuWatchdogThread> weak_ptr_;
  base::WeakPtrFactory<GpuWatchdogThread> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GpuWatchdogThread);
};

}  // namespace gpu
#endif  // GPU_IPC_SERVICE_GPU_WATCHDOG_THREAD_H_

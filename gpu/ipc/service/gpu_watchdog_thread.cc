// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_watchdog_thread.h"

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bit_cast.h"
#include "base/callback_helpers.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/native_library.h"
#include "base/numerics/safe_conversions.h"
#include "base/power_monitor/power_monitor.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/ipc/common/result_codes.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace gpu {
#if defined(OS_WIN)
base::TimeDelta GetGpuWatchdogTimeoutBasedOnCpuCores() {
  if (base::win::GetVersion() >= base::win::Version::WIN10) {
    int num_of_processors = base::SysInfo::NumberOfProcessors();

    if (num_of_processors > 8)
      return (kGpuWatchdogTimeout - base::TimeDelta::FromSeconds(10));
    else if (num_of_processors <= 4)
      return kGpuWatchdogTimeout + base::TimeDelta::FromSeconds(5);
  }

  return kGpuWatchdogTimeout;
}
#endif

GpuWatchdogThread::GpuWatchdogThread(base::TimeDelta timeout,
                                     int init_factor,
                                     int restart_factor,
                                     bool is_test_mode)
    : base::Thread("GpuWatchdog"),
      watchdog_timeout_(timeout),
      watchdog_init_factor_(init_factor),
      watchdog_restart_factor_(restart_factor),
      in_gpu_initialization_(true),
      is_test_mode_(is_test_mode),
      watched_gpu_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  base::CurrentThread::Get()->AddTaskObserver(this);
  num_of_processors_ = base::SysInfo::NumberOfProcessors();

#if defined(OS_WIN)
  // GetCurrentThread returns a pseudo-handle that cannot be used by one thread
  // to identify another. DuplicateHandle creates a "real" handle that can be
  // used for this purpose.
  if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                       GetCurrentProcess(), &watched_thread_handle_,
                       THREAD_QUERY_INFORMATION, FALSE, 0)) {
    watched_thread_handle_ = nullptr;
  }
#endif

#if defined(USE_X11)
  tty_file_ = base::OpenFile(
      base::FilePath(FILE_PATH_LITERAL("/sys/class/tty/tty0/active")), "r");
  UpdateActiveTTY();
  host_tty_ = active_tty_;
#endif

  Arm();
}

GpuWatchdogThread::~GpuWatchdogThread() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());
  // Stop() might take too long and the watchdog timeout is triggered.
  // Disarm first before calling Stop() to avoid a crash.
  if (IsArmed())
    Disarm();
  PauseWatchdog();

  Stop();  // stop the watchdog thread

  base::CurrentThread::Get()->RemoveTaskObserver(this);
  base::PowerMonitor::RemoveObserver(this);
  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogEnd);
#if defined(OS_WIN)
  if (watched_thread_handle_)
    CloseHandle(watched_thread_handle_);
#endif

#if defined(USE_X11)
  if (tty_file_)
    fclose(tty_file_);
#endif
}

// static
std::unique_ptr<GpuWatchdogThread> GpuWatchdogThread::Create(
    bool start_backgrounded,
    base::TimeDelta timeout,
    int init_factor,
    int restart_factor,
    bool is_test_mode) {
  auto watchdog_thread = base::WrapUnique(new GpuWatchdogThread(
      timeout, init_factor, restart_factor, is_test_mode));
  base::Thread::Options options;
  options.timer_slack = base::TIMER_SLACK_MAXIMUM;
  watchdog_thread->StartWithOptions(options);
  if (start_backgrounded)
    watchdog_thread->OnBackgrounded();
  return watchdog_thread;
}

// static
std::unique_ptr<GpuWatchdogThread> GpuWatchdogThread::Create(
    bool start_backgrounded) {
#if defined(OS_WIN)
  base::TimeDelta gpu_watchdog_timeout = GetGpuWatchdogTimeoutBasedOnCpuCores();
#else
  base::TimeDelta gpu_watchdog_timeout = kGpuWatchdogTimeout;
#endif

  return Create(start_backgrounded, gpu_watchdog_timeout, kInitFactor,
                kRestartFactor, false);
}

// Do not add power observer during watchdog init, PowerMonitor might not be up
// running yet.
void GpuWatchdogThread::AddPowerObserver() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  // Forward it to the watchdog thread. Call PowerMonitor::AddObserver on the
  // watchdog thread so that OnSuspend and OnResume will be called on watchdog
  // thread.
  is_add_power_observer_called_ = true;
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&GpuWatchdogThread::OnAddPowerObserver,
                                         base::Unretained(this)));
}

// Android Chrome goes to the background. Called from the gpu thread.
void GpuWatchdogThread::OnBackgrounded() {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThread::StopWatchdogTimeoutTask,
                     base::Unretained(this), kAndroidBackgroundForeground));
}

// Android Chrome goes to the foreground. Called from the gpu thread.
void GpuWatchdogThread::OnForegrounded() {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThread::RestartWatchdogTimeoutTask,
                     base::Unretained(this), kAndroidBackgroundForeground));
}

// Called from the gpu thread when gpu init has completed.
void GpuWatchdogThread::OnInitComplete() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuWatchdogThread::UpdateInitializationFlag,
                                base::Unretained(this)));
  Disarm();
}

// Called from the gpu thread in viz::GpuServiceImpl::~GpuServiceImpl().
// After this, no Disarm() will be called before the watchdog thread is
// destroyed. If this destruction takes too long, the watchdog timeout
// will be triggered.
void GpuWatchdogThread::OnGpuProcessTearDown() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  in_gpu_process_teardown_ = true;
  if (!IsArmed())
    Arm();
}

// Called from the gpu main thread.
void GpuWatchdogThread::PauseWatchdog() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuWatchdogThread::StopWatchdogTimeoutTask,
                                base::Unretained(this), kGeneralGpuFlow));
}

// Called from the gpu main thread.
void GpuWatchdogThread::ResumeWatchdog() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuWatchdogThread::RestartWatchdogTimeoutTask,
                                base::Unretained(this), kGeneralGpuFlow));
}

// Running on the watchdog thread.
// On Linux, Init() will be called twice for Sandbox Initialization. The
// watchdog is stopped and then restarted in StartSandboxLinux(). Everything
// should be the same and continue after the second init().
void GpuWatchdogThread::Init() {
  watchdog_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  // Get and Invalidate weak_ptr should be done on the watchdog thread only.
  weak_ptr_ = weak_factory_.GetWeakPtr();
  base::TimeDelta timeout = watchdog_timeout_ * kInitFactor;
  task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThread::OnWatchdogTimeout, weak_ptr_),
      timeout);

  last_arm_disarm_counter_ = ReadArmDisarmCounter();
  watchdog_start_timeticks_ = base::TimeTicks::Now();
  last_on_watchdog_timeout_timeticks_ = watchdog_start_timeticks_;
  next_on_watchdog_timeout_time_ = base::Time::Now() + timeout;

#if defined(OS_WIN)
  if (watched_thread_handle_) {
    if (base::ThreadTicks::IsSupported())
      base::ThreadTicks::WaitUntilInitialized();
    last_on_watchdog_timeout_thread_ticks_ = GetWatchedThreadTime();
    remaining_watched_thread_ticks_ = timeout;
  }
#endif
}

// Running on the watchdog thread.
void GpuWatchdogThread::CleanUp() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  weak_factory_.InvalidateWeakPtrs();
}

void GpuWatchdogThread::ReportProgress() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());
  InProgress();
}

void GpuWatchdogThread::WillProcessTask(const base::PendingTask& pending_task,
                                        bool was_blocked_or_low_priority) {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  // The watchdog is armed at the beginning of the gpu process teardown.
  // Do not call Arm() during teardown.
  if (in_gpu_process_teardown_)
    DCHECK(IsArmed());
  else
    Arm();
}

void GpuWatchdogThread::DidProcessTask(const base::PendingTask& pending_task) {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  // Keep the watchdog armed during tear down.
  if (in_gpu_process_teardown_)
    InProgress();
  else
    Disarm();
}

// Power Suspends. Running on the watchdog thread.
void GpuWatchdogThread::OnSuspend() {
  StopWatchdogTimeoutTask(kPowerSuspendResume);
}

// Power Resumes. Running on the watchdog thread.
void GpuWatchdogThread::OnResume() {
  RestartWatchdogTimeoutTask(kPowerSuspendResume);
}

// Running on the watchdog thread.
void GpuWatchdogThread::OnAddPowerObserver() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(base::PowerMonitor::IsInitialized());

  base::PowerMonitor::AddObserver(this);
  is_power_observer_added_ = true;
}

// Running on the watchdog thread.
void GpuWatchdogThread::RestartWatchdogTimeoutTask(
    PauseResumeSource source_of_request) {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  base::TimeDelta timeout;

  switch (source_of_request) {
    case kAndroidBackgroundForeground:
      if (!is_backgrounded_)
        return;
      is_backgrounded_ = false;
      timeout = watchdog_timeout_ * watchdog_restart_factor_;
      foregrounded_timeticks_ = base::TimeTicks::Now();
      foregrounded_event_ = true;
      num_of_timeout_after_foregrounded_ = 0;
      break;
    case kPowerSuspendResume:
      if (!in_power_suspension_)
        return;
      in_power_suspension_ = false;
      timeout = watchdog_timeout_ * watchdog_restart_factor_;
      power_resume_timeticks_ = base::TimeTicks::Now();
      power_resumed_event_ = true;
      num_of_timeout_after_power_resume_ = 0;
      break;
    case kGeneralGpuFlow:
      if (!is_paused_)
        return;
      is_paused_ = false;
      timeout = watchdog_timeout_ * watchdog_init_factor_;
      watchdog_resume_timeticks_ = base::TimeTicks::Now();
      break;
  }

  if (!is_backgrounded_ && !in_power_suspension_ && !is_paused_) {
    weak_ptr_ = weak_factory_.GetWeakPtr();
    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GpuWatchdogThread::OnWatchdogTimeout, weak_ptr_),
        timeout);
    last_on_watchdog_timeout_timeticks_ = base::TimeTicks::Now();
    next_on_watchdog_timeout_time_ = base::Time::Now() + timeout;
    last_arm_disarm_counter_ = ReadArmDisarmCounter();
#if defined(OS_WIN)
    if (watched_thread_handle_) {
      last_on_watchdog_timeout_thread_ticks_ = GetWatchedThreadTime();
      remaining_watched_thread_ticks_ = timeout;
    }
#endif
  }
}

void GpuWatchdogThread::StopWatchdogTimeoutTask(
    PauseResumeSource source_of_request) {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());

  switch (source_of_request) {
    case kAndroidBackgroundForeground:
      if (is_backgrounded_)
        return;
      is_backgrounded_ = true;
      backgrounded_timeticks_ = base::TimeTicks::Now();
      foregrounded_event_ = false;
      break;
    case kPowerSuspendResume:
      if (in_power_suspension_)
        return;
      in_power_suspension_ = true;
      power_suspend_timeticks_ = base::TimeTicks::Now();
      power_resumed_event_ = false;
      break;
    case kGeneralGpuFlow:
      if (is_paused_)
        return;
      is_paused_ = true;
      watchdog_pause_timeticks_ = base::TimeTicks::Now();
      break;
  }

  // Revoke any pending watchdog timeout task
  weak_factory_.InvalidateWeakPtrs();
}

void GpuWatchdogThread::UpdateInitializationFlag() {
  in_gpu_initialization_ = false;
}

// Called from the gpu main thread.
// The watchdog is armed only in these three functions -
// GpuWatchdogThread(), WillProcessTask(), and OnGpuProcessTearDown()
void GpuWatchdogThread::Arm() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  base::subtle::NoBarrier_AtomicIncrement(&arm_disarm_counter_, 1);

  // Arm/Disarm are always called in sequence. Now it's an odd number.
  DCHECK(IsArmed());
}

void GpuWatchdogThread::Disarm() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  base::subtle::NoBarrier_AtomicIncrement(&arm_disarm_counter_, 1);

  // Arm/Disarm are always called in sequence. Now it's an even number.
  DCHECK(!IsArmed());
}

void GpuWatchdogThread::InProgress() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  // Increment by 2. This is equivalent to Disarm() + Arm().
  base::subtle::NoBarrier_AtomicIncrement(&arm_disarm_counter_, 2);

  // Now it's an odd number.
  DCHECK(IsArmed());
}

bool GpuWatchdogThread::IsArmed() {
  // It's an odd number.
  return base::subtle::NoBarrier_Load(&arm_disarm_counter_) & 1;
}

base::subtle::Atomic32 GpuWatchdogThread::ReadArmDisarmCounter() {
  return base::subtle::NoBarrier_Load(&arm_disarm_counter_);
}

// Running on the watchdog thread.
void GpuWatchdogThread::OnWatchdogTimeout() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!is_backgrounded_);
  DCHECK(!in_power_suspension_);
  DCHECK(!is_paused_);

  // If this metric is added too early (eg. watchdog creation time), it cannot
  // be persistent. The histogram data will be lost after crash or browser exit.
  // Delay the recording of kGpuWatchdogStart until the firs
  // OnWatchdogTimeout() to ensure this metric is created in the persistent
  // memory.
  if (!is_watchdog_start_histogram_recorded) {
    is_watchdog_start_histogram_recorded = true;
    GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogStart);
  }

  auto arm_disarm_counter = ReadArmDisarmCounter();
  GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kTimeout);
  if (power_resumed_event_)
    num_of_timeout_after_power_resume_++;
  if (foregrounded_event_)
    num_of_timeout_after_foregrounded_++;

#if defined(USE_X11)
  UpdateActiveTTY();
#endif

  // Collect all needed info for gpu hang detection.
  bool disarmed = arm_disarm_counter % 2 == 0;  // even number
  bool gpu_makes_progress = arm_disarm_counter != last_arm_disarm_counter_;
  bool no_gpu_hang = disarmed || gpu_makes_progress || SlowWatchdogThread();

  bool watched_thread_needs_more_time =
      WatchedThreadNeedsMoreThreadTime(no_gpu_hang);
  no_gpu_hang = no_gpu_hang || watched_thread_needs_more_time ||
                ContinueOnNonHostX11ServerTty();

  // No gpu hang. Continue with another OnWatchdogTimeout task.
  if (no_gpu_hang) {
    last_on_watchdog_timeout_timeticks_ = base::TimeTicks::Now();
    next_on_watchdog_timeout_time_ = base::Time::Now() + watchdog_timeout_;
    last_arm_disarm_counter_ = ReadArmDisarmCounter();

    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GpuWatchdogThread::OnWatchdogTimeout, weak_ptr_),
        watchdog_timeout_);
    return;
  }

  // Still armed without any progress. GPU possibly hangs.
  GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kKill);
#if defined(OS_WIN)
  if (less_than_full_thread_time_after_capped_)
    GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kKillOnLessThreadTime);
#endif

  DeliberatelyTerminateToRecoverFromHang();
}

bool GpuWatchdogThread::SlowWatchdogThread() {
  // If it takes 15 more seconds than the expected time between two
  // OnWatchdogTimeout() calls, the system is considered slow and it's not a GPU
  // hang.
  bool slow_watchdog_thread =
      (base::Time::Now() - next_on_watchdog_timeout_time_) >=
      base::TimeDelta::FromSeconds(15);

  // Record this case only when a GPU hang is detected and the thread is slow.
  if (slow_watchdog_thread)
    GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kSlowWatchdogThread);

  return slow_watchdog_thread;
}

bool GpuWatchdogThread::WatchedThreadNeedsMoreThreadTime(
    bool no_gpu_hang_detected) {
#if defined(OS_WIN)
  if (!watched_thread_handle_)
    return false;

  // We allow extra thread time. When that runs out, we extend extra timeout
  // cycles. Now, we are extending extra timeout cycles. Don't add extra thread
  // time.
  if (count_of_extra_cycles_ > 0)
    return false;

  WatchedThreadNeedsMoreThreadTimeHistogram(
      no_gpu_hang_detected,
      /*start_of_more_thread_time*/ false);

  if (!no_gpu_hang_detected && count_of_more_gpu_thread_time_allowed_ >=
                                   kMaxCountOfMoreGpuThreadTimeAllowed) {
    less_than_full_thread_time_after_capped_ = true;
  } else {
    less_than_full_thread_time_after_capped_ = false;
  }

  // Calculate how many thread ticks the watched thread spent doing the work.
  base::ThreadTicks now = GetWatchedThreadTime();
  base::TimeDelta thread_time_elapsed =
      now - last_on_watchdog_timeout_thread_ticks_;
  last_on_watchdog_timeout_thread_ticks_ = now;
  remaining_watched_thread_ticks_ -= thread_time_elapsed;

  if (no_gpu_hang_detected ||
      count_of_more_gpu_thread_time_allowed_ >=
          kMaxCountOfMoreGpuThreadTimeAllowed ||
      thread_time_elapsed < base::TimeDelta() /* bogus data */ ||
      remaining_watched_thread_ticks_ <= base::TimeDelta()) {
    // Reset the remaining thread ticks.
    remaining_watched_thread_ticks_ = watchdog_timeout_;
    count_of_more_gpu_thread_time_allowed_ = 0;

    return false;
  } else {
    // This is the start of allowing more thread time.
    if (count_of_more_gpu_thread_time_allowed_ == 0) {
      WatchedThreadNeedsMoreThreadTimeHistogram(
          no_gpu_hang_detected, /*start_of_more_thread_time*/ true);
    }
    count_of_more_gpu_thread_time_allowed_++;

    return true;
  }
#else
  return false;
#endif
}

#if defined(OS_WIN)
base::ThreadTicks GpuWatchdogThread::GetWatchedThreadTime() {
  DCHECK(watched_thread_handle_);

  if (base::ThreadTicks::IsSupported()) {
    // Note: GetForThread() might return bogus results if running on different
    // CPUs between two calls.
    return base::ThreadTicks::GetForThread(
        base::PlatformThreadHandle(watched_thread_handle_));
  } else {
    FILETIME creation_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    BOOL result = GetThreadTimes(watched_thread_handle_, &creation_time,
                                 &exit_time, &kernel_time, &user_time);
    if (!result)
      return base::ThreadTicks();

    // Need to bit_cast to fix alignment, then divide by 10 to convert
    // 100-nanoseconds to microseconds.
    int64_t user_time_us = bit_cast<int64_t, FILETIME>(user_time) / 10;
    int64_t kernel_time_us = bit_cast<int64_t, FILETIME>(kernel_time) / 10;

    return base::ThreadTicks() +
           base::TimeDelta::FromMicroseconds(user_time_us + kernel_time_us);
  }
}
#endif

void GpuWatchdogThread::DeliberatelyTerminateToRecoverFromHang() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  // If this is for gpu testing, do not terminate the gpu process.
  if (is_test_mode_) {
    test_result_timeout_and_gpu_hang_.Set();
    return;
  }

#if defined(OS_WIN)
  if (IsDebuggerPresent())
    return;
#endif

  // Store variables so they're available in crash dumps to help determine the
  // cause of any hang.
  base::TimeTicks function_begin_timeticks = base::TimeTicks::Now();
  base::debug::Alias(&in_gpu_initialization_);
  base::debug::Alias(&num_of_timeout_after_power_resume_);
  base::debug::Alias(&num_of_timeout_after_foregrounded_);
  base::debug::Alias(&function_begin_timeticks);
  base::debug::Alias(&watchdog_start_timeticks_);
  base::debug::Alias(&power_suspend_timeticks_);
  base::debug::Alias(&power_resume_timeticks_);
  base::debug::Alias(&backgrounded_timeticks_);
  base::debug::Alias(&foregrounded_timeticks_);
  base::debug::Alias(&watchdog_pause_timeticks_);
  base::debug::Alias(&watchdog_resume_timeticks_);
  base::debug::Alias(&in_power_suspension_);
  base::debug::Alias(&in_gpu_process_teardown_);
  base::debug::Alias(&is_backgrounded_);
  base::debug::Alias(&is_add_power_observer_called_);
  base::debug::Alias(&is_power_observer_added_);
  base::debug::Alias(&last_on_watchdog_timeout_timeticks_);
  base::TimeDelta timeticks_elapses =
      function_begin_timeticks - last_on_watchdog_timeout_timeticks_;
  base::debug::Alias(&timeticks_elapses);
#if defined(OS_WIN)
  base::debug::Alias(&remaining_watched_thread_ticks_);
  base::debug::Alias(&less_than_full_thread_time_after_capped_);
#endif

  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogKill);

  crash_keys::gpu_watchdog_crashed_in_gpu_init.Set(
      in_gpu_initialization_ ? "1" : "0");

  crash_keys::gpu_watchdog_kill_after_power_resume.Set(
      WithinOneMinFromPowerResumed() ? "1" : "0");

  crash_keys::num_of_processors.Set(base::NumberToString(num_of_processors_));

  // Check the arm_disarm_counter value one more time.
  auto last_arm_disarm_counter = ReadArmDisarmCounter();
  base::debug::Alias(&last_arm_disarm_counter);

  // Use RESULT_CODE_HUNG so this crash is separated from other
  // EXCEPTION_ACCESS_VIOLATION buckets for UMA analysis.
  // Create a crash dump first. TerminateCurrentProcessImmediately will not
  // create a dump.
  base::debug::DumpWithoutCrashing();
  base::Process::TerminateCurrentProcessImmediately(RESULT_CODE_HUNG);
}

void GpuWatchdogThread::GpuWatchdogHistogram(
    GpuWatchdogThreadEvent thread_event) {
  base::UmaHistogramEnumeration("GPU.WatchdogThread.Event", thread_event);
}

void GpuWatchdogThread::GpuWatchdogTimeoutHistogram(
    GpuWatchdogTimeoutEvent timeout_event) {
  base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout", timeout_event);

  bool recorded = false;
  if (in_gpu_initialization_) {
    base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout.Init",
                                  timeout_event);
    recorded = true;
  }

  if (WithinOneMinFromPowerResumed()) {
    base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout.PowerResume",
                                  timeout_event);
    recorded = true;
  }

  if (WithinOneMinFromForegrounded()) {
    base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout.Foregrounded",
                                  timeout_event);
    recorded = true;
  }

  if (!recorded) {
    base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout.Normal",
                                  timeout_event);
  }
}

#if defined(OS_WIN)
void GpuWatchdogThread::RecordExtraThreadTimeHistogram() {
  // Record the number of timeouts the GPU main thread needs to make a progress
  // after GPU OnWatchdogTimeout() is triggered. The maximum count is 6 which
  // is more  than kMaxCountOfMoreGpuThreadTimeAllowed(4);
  constexpr int kMin = 1;
  constexpr int kMax = 6;
  constexpr int kBuckets = 6;
  int count = count_of_more_gpu_thread_time_allowed_;
  bool recorded = false;

  base::UmaHistogramCustomCounts("GPU.WatchdogThread.ExtraThreadTime", count,
                                 kMin, kMax, kBuckets);

  if (in_gpu_initialization_) {
    base::UmaHistogramCustomCounts("GPU.WatchdogThread.ExtraThreadTime.Init",
                                   count, kMin, kMax, kBuckets);
    recorded = true;
  }

  if (WithinOneMinFromPowerResumed()) {
    base::UmaHistogramCustomCounts(
        "GPU.WatchdogThread.ExtraThreadTime.PowerResume", count, kMin, kMax,
        kBuckets);
    recorded = true;
  }

  if (WithinOneMinFromForegrounded()) {
    base::UmaHistogramCustomCounts(
        "GPU.WatchdogThread.ExtraThreadTime.Foregrounded", count, kMin, kMax,
        kBuckets);
    recorded = true;
  }

  if (!recorded) {
    base::UmaHistogramCustomCounts("GPU.WatchdogThread.ExtraThreadTime.Normal",
                                   count, kMin, kMax, kBuckets);
  }
}

void GpuWatchdogThread::RecordNumOfUsersWaitingWithExtraThreadTimeHistogram(
    int count) {
  constexpr int kMax = 4;

  base::UmaHistogramExactLinear("GPU.WatchdogThread.ExtraThreadTime.NumOfUsers",
                                count, kMax);
}

void GpuWatchdogThread::WatchedThreadNeedsMoreThreadTimeHistogram(
    bool no_gpu_hang_detected,
    bool start_of_more_thread_time) {
  if (start_of_more_thread_time) {
    // This is the start of allowing more thread time. Only record it once for
    // all following timeouts on the same detected gpu hang, so we know this
    // is equivlent one crash in our crash reports.
    GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kMoreThreadTime);
    RecordNumOfUsersWaitingWithExtraThreadTimeHistogram(0);
  } else {
    if (count_of_more_gpu_thread_time_allowed_ > 0) {
      if (no_gpu_hang_detected) {
        // If count_of_more_gpu_thread_time_allowed_ > 0, we know extra time was
        // extended in the previous OnWatchdogTimeout(). Now we find gpu makes
        // progress. Record this case.
        GpuWatchdogTimeoutHistogram(
            GpuWatchdogTimeoutEvent::kProgressAfterMoreThreadTime);
        RecordExtraThreadTimeHistogram();
      } else {
        if (count_of_more_gpu_thread_time_allowed_ >=
            kMaxCountOfMoreGpuThreadTimeAllowed) {
          GpuWatchdogTimeoutHistogram(
              GpuWatchdogTimeoutEvent::kLessThanFullThreadTimeAfterCapped);
        }
      }

      // Records the number of users who are still waiting. We can use this
      // number to calculate the number of users who had already quit.
      RecordNumOfUsersWaitingWithExtraThreadTimeHistogram(
          count_of_more_gpu_thread_time_allowed_);

      // Used by GPU.WatchdogThread.WaitTime later
      time_in_wait_for_full_thread_time_ =
          count_of_more_gpu_thread_time_allowed_ * watchdog_timeout_;
    }
  }
}
#endif

bool GpuWatchdogThread::WithinOneMinFromPowerResumed() {
  size_t count = base::ClampFloor<size_t>(base::TimeDelta::FromMinutes(1) /
                                          watchdog_timeout_);
  return power_resumed_event_ && num_of_timeout_after_power_resume_ <= count;
}

bool GpuWatchdogThread::WithinOneMinFromForegrounded() {
  size_t count = base::ClampFloor<size_t>(base::TimeDelta::FromMinutes(1) /
                                          watchdog_timeout_);
  return foregrounded_event_ && num_of_timeout_after_foregrounded_ <= count;
}

#if defined(USE_X11)
void GpuWatchdogThread::UpdateActiveTTY() {
  last_active_tty_ = active_tty_;

  active_tty_ = -1;
  char tty_string[8] = {0};
  if (tty_file_ && !fseek(tty_file_, 0, SEEK_SET) &&
      fread(tty_string, 1, 7, tty_file_)) {
    int tty_number;
    if (sscanf(tty_string, "tty%d\n", &tty_number) == 1) {
      active_tty_ = tty_number;
    }
  }
}
#endif

bool GpuWatchdogThread::ContinueOnNonHostX11ServerTty() {
#if defined(USE_X11)
  if (host_tty_ == -1 || active_tty_ == -1)
    return false;

  // Don't crash if we're not on the TTY of our host X11 server.
  if (active_tty_ != host_tty_) {
    // Only record for the time there is a change on TTY
    if (last_active_tty_ == active_tty_) {
      GpuWatchdogTimeoutHistogram(
          GpuWatchdogTimeoutEvent::kContinueOnNonHostServerTty);
    }
    return true;
  }
#endif
  return false;
}

// For gpu testing only. Return whether a GPU hang was detected or not.
bool GpuWatchdogThread::IsGpuHangDetectedForTesting() {
  DCHECK(is_test_mode_);
  return test_result_timeout_and_gpu_hang_.IsSet();
}

// This should be called on the test main thread only. It will wait until the
// power observer is added on the watchdog thread.
void GpuWatchdogThread::WaitForPowerObserverAddedForTesting() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(is_add_power_observer_called_);

  // Just return if it has been added.
  if (is_power_observer_added_)
    return;

  base::WaitableEvent event;
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));
  event.Wait();
}

}  // namespace gpu

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_watchdog_thread.h"

#include <memory>
#include <string>
#include <utility>

#include "base/atomicops.h"
#include "base/bit_cast.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/native_library.h"
#include "base/numerics/safe_conversions.h"
#include "base/power_monitor/power_monitor.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/result_codes.h"

namespace gpu {

base::TimeDelta GetGpuWatchdogTimeout(bool software_rendering) {
  std::string timeout_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kGpuWatchdogTimeoutSeconds);
  if (!timeout_str.empty()) {
    size_t timeout_seconds;
    if (base::StringToSizeT(timeout_str, &timeout_seconds))
      return base::Seconds(timeout_seconds);

    LOG(WARNING) << "Invalid --" << switches::kGpuWatchdogTimeoutSeconds << ": "
                 << timeout_str;
  }

  base::TimeDelta timeout = kGpuWatchdogTimeout;
#if BUILDFLAG(IS_WIN)
  int num_of_processors = base::SysInfo::NumberOfProcessors();
  if (num_of_processors > 8) {
    timeout -= base::Seconds(10);
  } else if (num_of_processors <= 4) {
    timeout += base::Seconds(5);
  }
#endif

  if (software_rendering) {
    timeout *= kSoftwareRenderingFactor;
  }
  return timeout;
}

GpuWatchdogThread::GpuWatchdogThread(base::TimeDelta timeout,
                                     int restart_factor,
                                     bool is_test_mode,
                                     const std::string& thread_name)
    : base::Thread(thread_name),
      watchdog_timeout_(timeout),
      watchdog_restart_factor_(restart_factor),
      is_test_mode_(is_test_mode) {
  base::CurrentThread::Get()->AddTaskObserver(this);

  // DO NOT CHANGE |watched_thread_name_str_uma_|. It's used for UMA and crash
  // report.
  if (thread_name == "GpuWatchdog_Compositor")
    watched_thread_name_str_uma_ = ".compositor";
  else
    watched_thread_name_str_uma_ = ".main";

#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/40187449): Remove this once macOS uses system-wide ids.
  // On macOS the thread ids used by CrashPad are not the same as the ones
  // provided by PlatformThread
  uint64_t watched_thread_id;
  pthread_threadid_np(pthread_self(), &watched_thread_id);
  watched_thread_id_str_ = base::NumberToString(watched_thread_id);
#else
  watched_thread_id_str_ =
      base::NumberToString(base::PlatformThread::CurrentId());
#endif

#if BUILDFLAG(IS_WIN)
  // GetCurrentThread returns a pseudo-handle that cannot be used by one thread
  // to identify another. DuplicateHandle creates a "real" handle that can be
  // used for this purpose.
  if (!::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentThread(),
                         ::GetCurrentProcess(), &watched_thread_handle_,
                         THREAD_QUERY_INFORMATION, FALSE, 0)) {
    watched_thread_handle_ = nullptr;
  }
#endif

#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)
  tty_file_.reset(base::OpenFile(
      base::FilePath(FILE_PATH_LITERAL("/sys/class/tty/tty0/active")), "r"));
  UpdateActiveTTY();
  host_tty_ = active_tty_;
#endif

  Arm();
}

GpuWatchdogThread::~GpuWatchdogThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watched_thread_sequence_checker_);
  // Stop() might take too long and the watchdog timeout is triggered.
  // Disarm first before calling Stop() to avoid a crash.
  if (IsArmed())
    Disarm();
  PauseWatchdog();

  Stop();  // stop the watchdog thread

  base::CurrentThread::Get()->RemoveTaskObserver(this);
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
  GpuWatchdogThreadEventHistogram(GpuWatchdogThreadEvent::kGpuWatchdogEnd);
#if BUILDFLAG(IS_WIN)
  if (watched_thread_handle_)
    CloseHandle(watched_thread_handle_);
#endif
}

// static
std::unique_ptr<GpuWatchdogThread> GpuWatchdogThread::Create(
    bool start_backgrounded,
    base::TimeDelta timeout,
    int restart_factor,
    bool is_test_mode,
    const std::string& thread_name) {
  auto watchdog_thread = base::WrapUnique(new GpuWatchdogThread(
      timeout, restart_factor, is_test_mode, thread_name));
  watchdog_thread->Start();
  if (start_backgrounded)
    watchdog_thread->OnBackgrounded();
  return watchdog_thread;
}

// static
std::unique_ptr<GpuWatchdogThread> GpuWatchdogThread::Create(
    bool start_backgrounded,
    bool software_rendering,
    const std::string& thread_name) {
  return Create(start_backgrounded, GetGpuWatchdogTimeout(software_rendering),
                kRestartFactor, /*test_mode=*/false, thread_name);
}

// static
std::unique_ptr<GpuWatchdogThread> GpuWatchdogThread::Create(
    bool start_backgrounded,
    const GpuWatchdogThread* existing_watchdog,
    const std::string& thread_name) {
  DCHECK(existing_watchdog);
  return Create(start_backgrounded, existing_watchdog->watchdog_timeout_,
                existing_watchdog->watchdog_restart_factor_,
                /*test_mode=*/false, thread_name);
}

// Android Chrome goes to the background. Called from the gpu io thread.
void GpuWatchdogThread::OnBackgrounded() {
  // Report progress first in case the Watchdog timeout task in the watchdog
  // thread is not invalidated soon enough.
  InProgress();

  {
    base::AutoLock lock(skip_lock_);
    skip_for_backgrounded_ = true;
  }

  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThread::StopWatchdogTimeoutTask,
                     base::Unretained(this), kAndroidBackgroundForeground));
}

// Android Chrome goes to the foreground. Called from the gpu io thread.
void GpuWatchdogThread::OnForegrounded() {
  {
    base::AutoLock lock(skip_lock_);
    skip_for_backgrounded_ = false;
  }

  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThread::RestartWatchdogTimeoutTask,
                     base::Unretained(this), kAndroidBackgroundForeground));
}

// Called from the gpu thread when gpu init has completed.
void GpuWatchdogThread::OnInitComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watched_thread_sequence_checker_);

  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuWatchdogThread::UpdateInitializationFlag,
                                base::Unretained(this)));
  Disarm();

  // The PowerMonitorObserver needs to be register on the watchdog thread so the
  // notifications are delivered on that thread.
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&GpuWatchdogThread::AddPowerObserver,
                                         base::Unretained(this)));
}

// Called from the gpu thread in viz::GpuServiceImpl::~GpuServiceImpl().
// After this, no Disarm() will be called before the watchdog thread is
// destroyed. If this destruction takes too long, the watchdog timeout
// will be triggered.
void GpuWatchdogThread::OnGpuProcessTearDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watched_thread_sequence_checker_);

  in_gpu_process_teardown_ = true;
  if (!IsArmed())
    Arm();
}

// Called from the watched gpu thread.
void GpuWatchdogThread::PauseWatchdog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watched_thread_sequence_checker_);
  // Report progress first in case the Watchdog timeout task in the watchdog
  // thread is not invalidated soon enough.
  InProgress();

  // From the crash report, |skip_for_pause_| along is not enough to prevent
  // GpuWatchdog kill after pause. If InProgress() along can prevent GpuWatchdog
  // kill, we might not need |skip_for_pause_|.
  {
    base::AutoLock lock(skip_lock_);
    skip_for_pause_ = true;
  }

  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuWatchdogThread::StopWatchdogTimeoutTask,
                                base::Unretained(this), kGeneralGpuFlow));
}

// Called from the watched gpu thread.
void GpuWatchdogThread::ResumeWatchdog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watched_thread_sequence_checker_);
  {
    base::AutoLock lock(skip_lock_);
    skip_for_pause_ = false;
  }

  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuWatchdogThread::RestartWatchdogTimeoutTask,
                                base::Unretained(this), kGeneralGpuFlow));
}

// Running on the watchdog thread.
// On Linux, Init() will be called twice for Sandbox Initialization. The
// watchdog is stopped and then restarted in StartSandboxLinux(). Everything
// should be the same and continue after the second init().
void GpuWatchdogThread::Init() {
  // Get and Invalidate weak_ptr should be done on the watchdog thread only.
  weak_ptr_ = weak_factory_.GetWeakPtr();
  task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThread::OnWatchdogTimeout, weak_ptr_),
      watchdog_timeout_);

  last_arm_disarm_counter_ = ReadArmDisarmCounter();
  watchdog_start_timeticks_ = base::TimeTicks::Now();
  last_on_watchdog_timeout_timeticks_ = watchdog_start_timeticks_;
  next_on_watchdog_timeout_time_ = base::Time::Now() + watchdog_timeout_;
  in_gpu_initialization_ = true;

#if BUILDFLAG(IS_WIN)
  if (watched_thread_handle_) {
    if (base::ThreadTicks::IsSupported())
      base::ThreadTicks::WaitUntilInitialized();
    last_on_watchdog_timeout_thread_ticks_ = GetWatchedThreadTime();
    remaining_watched_thread_ticks_ = watchdog_timeout_;
  }
#endif
}

// Running on the watchdog thread.
void GpuWatchdogThread::CleanUp() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  weak_factory_.InvalidateWeakPtrs();
}

void GpuWatchdogThread::ReportProgress() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watched_thread_sequence_checker_);
  InProgress();
}

void GpuWatchdogThread::WillProcessTask(const base::PendingTask& pending_task,
                                        bool was_blocked_or_low_priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watched_thread_sequence_checker_);

  // The watchdog is armed at the beginning of the gpu process teardown.
  // Do not call Arm() during teardown.
  if (in_gpu_process_teardown_)
    DCHECK(IsArmed());
  else
    Arm();
}

void GpuWatchdogThread::DidProcessTask(const base::PendingTask& pending_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watched_thread_sequence_checker_);
  // Keep the watchdog armed during tear down.
  if (in_gpu_process_teardown_)
    InProgress();
  else
    Disarm();
}

// Power Suspends. Running on the watchdog thread.
void GpuWatchdogThread::OnSuspend() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  InProgress();
  StopWatchdogTimeoutTask(kPowerSuspendResume);
}

// Power Resumes. Running on the watchdog thread.
void GpuWatchdogThread::OnResume() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  RestartWatchdogTimeoutTask(kPowerSuspendResume);
}

// Running on the watchdog thread.
// Call AddPowerSuspendObserver on the watchdog thread so that OnSuspend() and
// OnResume() will be called on this thread.
void GpuWatchdogThread::AddPowerObserver() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());

  // Adding the Observer to the power monitor is safe even if power monitor is
  // not yet initialized.
  bool is_system_suspended =
      base::PowerMonitor::GetInstance()
          ->AddPowerSuspendObserverAndReturnSuspendedState(this);
  if (is_system_suspended)
    StopWatchdogTimeoutTask(kPowerSuspendResume);
}

// Running on the watchdog thread.
void GpuWatchdogThread::RestartWatchdogTimeoutTask(
    PauseResumeSource source_of_request) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
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
      timeout = watchdog_timeout_;
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
#if BUILDFLAG(IS_WIN)
    if (watched_thread_handle_) {
      last_on_watchdog_timeout_thread_ticks_ = GetWatchedThreadTime();
      remaining_watched_thread_ticks_ = timeout;
    }
#endif
  }
}

void GpuWatchdogThread::StopWatchdogTimeoutTask(
    PauseResumeSource source_of_request) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());

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

// On the watchdog thread only.
void GpuWatchdogThread::UpdateInitializationFlag() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  in_gpu_initialization_ = false;
}

// Called from the watched gpu thread.
// The watchdog is armed only in these three functions -
// GpuWatchdogThread(), WillProcessTask(), and OnGpuProcessTearDown()
void GpuWatchdogThread::Arm() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watched_thread_sequence_checker_);

  base::subtle::NoBarrier_AtomicIncrement(&arm_disarm_counter_, 1);

  // Arm/Disarm are always called in sequence. Now it's an odd number.
  DCHECK(IsArmed());
}

void GpuWatchdogThread::Disarm() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watched_thread_sequence_checker_);

  base::subtle::NoBarrier_AtomicIncrement(&arm_disarm_counter_, 1);

  // Arm/Disarm are always called in sequence. Now it's an even number.
  DCHECK(!IsArmed());
}

void GpuWatchdogThread::InProgress() {
  // Increment by 2. This is equivalent to Disarm() + Arm().
  // If Watchdog is already disarmed, it stays in the same disarmed status.
  base::subtle::NoBarrier_AtomicIncrement(&arm_disarm_counter_, 2);
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
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!is_backgrounded_);
  DCHECK(!in_power_suspension_);
  DCHECK(!is_paused_);

  // If this metric is added too early (eg. watchdog creation time), it cannot
  // be persistent. The histogram data will be lost after crash or browser exit.
  // Delay the recording of kGpuWatchdogStart until the firs
  // OnWatchdogTimeout() to ensure this metric is created in the persistent
  // memory.
  if (!is_watchdog_start_histogram_recorded_) {
    is_watchdog_start_histogram_recorded_ = true;
    GpuWatchdogThreadEventHistogram(GpuWatchdogThreadEvent::kGpuWatchdogStart);
  }

  GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kTimeout);
  if (power_resumed_event_)
    num_of_timeout_after_power_resume_++;
  if (foregrounded_event_)
    num_of_timeout_after_foregrounded_++;

#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)
  UpdateActiveTTY();
#endif

  // Collect all needed info for gpu hang detection.
  auto arm_disarm_counter = ReadArmDisarmCounter();
  bool disarmed = arm_disarm_counter % 2 == 0;  // even number
  bool gpu_makes_progress = arm_disarm_counter != last_arm_disarm_counter_;
  bool no_gpu_hang = disarmed || gpu_makes_progress || SlowWatchdogThread();

  bool watched_thread_needs_more_time =
      WatchedThreadNeedsMoreThreadTime(no_gpu_hang);
  no_gpu_hang = no_gpu_hang || watched_thread_needs_more_time ||
                ContinueOnNonHostX11ServerTty();

  // Keep holding the lock until the end of this function so
  // DeliberatelyTerminateToRecoverFromHang() has the correct crash signature
  // if the kill is triggered before paused or backgrounded.
  base::AutoLock lock(skip_lock_);
  if (skip_for_pause_ || skip_for_backgrounded_) {
    no_gpu_hang = true;
  }

  // No gpu hang. Continue with another OnWatchdogTimeout task.
  if (no_gpu_hang) {
    ContinueWithNextWatchdogTimeoutTask();
    return;
  }

  // If the watched thread makes a progress after crash dump, the GPU process
  // will not be killed and every thing continues after this function.
  // Otherwise, this is the end of the GPU process.
  DeliberatelyTerminateToRecoverFromHang();
}

void GpuWatchdogThread::ContinueWithNextWatchdogTimeoutTask() {
  last_on_watchdog_timeout_timeticks_ = base::TimeTicks::Now();
  next_on_watchdog_timeout_time_ = base::Time::Now() + watchdog_timeout_;
  last_arm_disarm_counter_ = ReadArmDisarmCounter();

  task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThread::OnWatchdogTimeout, weak_ptr_),
      watchdog_timeout_);
}

bool GpuWatchdogThread::SlowWatchdogThread() {
  // If it takes 15 more seconds than the expected time between two
  // OnWatchdogTimeout() calls, the system is considered slow and it's not a GPU
  // hang.
  bool slow_watchdog_thread =
      (base::Time::Now() - next_on_watchdog_timeout_time_) >=
      kUnreasonableTimeoutDelay;

  // Record this case only when a GPU hang is detected and the thread is slow.
  if (slow_watchdog_thread)
    GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kSlowWatchdogThread);

  return slow_watchdog_thread;
}

bool GpuWatchdogThread::WatchedThreadNeedsMoreThreadTime(
    bool no_gpu_hang_detected) {
#if BUILDFLAG(IS_WIN)
  if (!watched_thread_handle_)
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
      thread_time_elapsed.is_negative() /* bogus data */ ||
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

#if BUILDFLAG(IS_WIN)
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
    int64_t user_time_us = base::bit_cast<int64_t, FILETIME>(user_time) / 10;
    int64_t kernel_time_us =
        base::bit_cast<int64_t, FILETIME>(kernel_time) / 10;

    return base::ThreadTicks() +
           base::Microseconds(user_time_us + kernel_time_us);
  }
}
#endif

void GpuWatchdogThread::DeliberatelyTerminateToRecoverFromHang() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());

  // If this is for gpu testing, do not terminate the gpu process.
  // Just signal and quit.
  if (is_test_mode_) {
    test_result_timeout_and_gpu_hang_.Set();
    return;
  }

#if BUILDFLAG(IS_WIN)
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
  base::debug::Alias(&skip_for_pause_);
  base::debug::Alias(&last_on_watchdog_timeout_timeticks_);
  base::TimeDelta timeticks_elapses =
      function_begin_timeticks - last_on_watchdog_timeout_timeticks_;
  base::debug::Alias(&timeticks_elapses);
#if BUILDFLAG(IS_WIN)
  base::debug::Alias(&remaining_watched_thread_ticks_);
  base::debug::Alias(&less_than_full_thread_time_after_capped_);
#endif

  // The watchdog currently doesn't watch multiple threads. If multiple threads
  // are supported, use '|' to separate thread ids in "list_of_hung_threads".
  crash_keys::list_of_hung_threads.Set(watched_thread_id_str_);

  crash_keys::gpu_watchdog_crashed_in_gpu_init.Set(
      in_gpu_initialization_ ? "1" : "0");

  crash_keys::gpu_watchdog_kill_after_power_resume.Set(
      WithinOneMinFromPowerResumed() ? "1" : "0");

  const int num_of_processors = base::SysInfo::NumberOfProcessors();
  crash_keys::num_of_processors.Set(base::NumberToString(num_of_processors));

  crash_keys::gpu_thread.Set(watched_thread_name_str_uma_);

  // Check the arm_disarm_counter value one more time.
  auto last_arm_disarm_counter = ReadArmDisarmCounter();
  base::debug::Alias(&last_arm_disarm_counter);

  // Create a crash dump first
  base::debug::DumpWithoutCrashing();

  // A kKill event is triggered and DumpWithoutCrashing() is called in the
  // watchdog timeout routine OnWatchdogTimeout(). If it turns out
  // gpu does not hang after the crash dump, another histogram
  // kNoKillForGpuProgressDuringCrashDumping will be recorded later.
  GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kKill);

  // Final check after the crash dump. If the watched thread makes a progress
  // (disarmed) during generating crash dump, no need to crash the GPU process.
  bool gpu_hang = IsArmed();
  if (gpu_hang) {
    // Still armed without any progress. The GPU process is now killed.
    GpuWatchdogThreadEventHistogram(GpuWatchdogThreadEvent::kGpuWatchdogKill);
#if BUILDFLAG(IS_WIN)
    if (less_than_full_thread_time_after_capped_)
      GpuWatchdogTimeoutHistogram(
          GpuWatchdogTimeoutEvent::kKillOnLessThreadTime);
#endif

    // Use RESULT_CODE_HUNG so this crash is separated from other
    // EXCEPTION_ACCESS_VIOLATION buckets for UMA analysis.
    // TerminateCurrentProcessImmediately itself will not generate a dump.
    base::Process::TerminateCurrentProcessImmediately(RESULT_CODE_HUNG);
    // The end of the GPU process.
  } else {
    crash_keys::list_of_hung_threads.Clear();
    crash_keys::gpu_watchdog_crashed_in_gpu_init.Clear();
    crash_keys::gpu_watchdog_kill_after_power_resume.Clear();
    crash_keys::num_of_processors.Clear();
    crash_keys::gpu_thread.Clear();

    GpuWatchdogTimeoutHistogram(
        GpuWatchdogTimeoutEvent::kNoKillForGpuProgressDuringCrashDumping);
#if BUILDFLAG(IS_WIN)
    // Reset the counters for WatchedThreadNeedsMoreThreadTime().
    remaining_watched_thread_ticks_ = watchdog_timeout_;
    count_of_more_gpu_thread_time_allowed_ = 0;
#endif

    ContinueWithNextWatchdogTimeoutTask();
  }
}

void GpuWatchdogThread::GpuWatchdogThreadEventHistogram(
    GpuWatchdogThreadEvent thread_event) {
  base::UmaHistogramEnumeration("GPU.WatchdogThread.Event", thread_event);
  base::UmaHistogramEnumeration(
      "GPU.WatchdogThread.Event" + watched_thread_name_str_uma_, thread_event);
}

void GpuWatchdogThread::GpuWatchdogTimeoutHistogram(
    GpuWatchdogTimeoutEvent timeout_event) {
  base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout", timeout_event);
  base::UmaHistogramEnumeration(
      "GPU.WatchdogThread.Timeout" + watched_thread_name_str_uma_,
      timeout_event);

  bool recorded = false;
  if (in_gpu_initialization_) {
    base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout.Init",
                                  timeout_event);
    base::UmaHistogramEnumeration(
        "GPU.WatchdogThread.Timeout.Init" + watched_thread_name_str_uma_,
        timeout_event);
    recorded = true;
  }

  if (WithinOneMinFromPowerResumed()) {
    base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout.PowerResume",
                                  timeout_event);
    base::UmaHistogramEnumeration(
        "GPU.WatchdogThread.Timeout.PowerResume" + watched_thread_name_str_uma_,
        timeout_event);
    recorded = true;
  }

  if (WithinOneMinFromForegrounded()) {
    base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout.Foregrounded",
                                  timeout_event);
    base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout.Foregrounded" +
                                      watched_thread_name_str_uma_,
                                  timeout_event);
    recorded = true;
  }

  if (!recorded) {
    base::UmaHistogramEnumeration("GPU.WatchdogThread.Timeout.Normal",
                                  timeout_event);
    base::UmaHistogramEnumeration(
        "GPU.WatchdogThread.Timeout.Normal" + watched_thread_name_str_uma_,
        timeout_event);
  }
}

#if BUILDFLAG(IS_WIN)
void GpuWatchdogThread::WatchedThreadNeedsMoreThreadTimeHistogram(
    bool no_gpu_hang_detected,
    bool start_of_more_thread_time) {
  if (start_of_more_thread_time) {
    // This is the start of allowing more thread time. Only record it once for
    // all following timeouts on the same detected gpu hang, so we know this
    // is equivalent one crash in our crash reports.
    GpuWatchdogTimeoutHistogram(GpuWatchdogTimeoutEvent::kMoreThreadTime);
  } else {
    if (count_of_more_gpu_thread_time_allowed_ > 0) {
      if (no_gpu_hang_detected) {
        // If count_of_more_gpu_thread_time_allowed_ > 0, we know extra time was
        // extended in the previous OnWatchdogTimeout(). Now we find gpu makes
        // progress. Record this case.
        GpuWatchdogTimeoutHistogram(
            GpuWatchdogTimeoutEvent::kProgressAfterMoreThreadTime);
      } else if (count_of_more_gpu_thread_time_allowed_ >=
                 kMaxCountOfMoreGpuThreadTimeAllowed) {
        GpuWatchdogTimeoutHistogram(
            GpuWatchdogTimeoutEvent::kLessThanFullThreadTimeAfterCapped);
      }
    }
  }
}
#endif

bool GpuWatchdogThread::WithinOneMinFromPowerResumed() {
  size_t count = base::ClampFloor<size_t>(base::Minutes(1) / watchdog_timeout_);
  return power_resumed_event_ && num_of_timeout_after_power_resume_ <= count;
}

bool GpuWatchdogThread::WithinOneMinFromForegrounded() {
  size_t count = base::ClampFloor<size_t>(base::Minutes(1) / watchdog_timeout_);
  return foregrounded_event_ && num_of_timeout_after_foregrounded_ <= count;
}

#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)
void GpuWatchdogThread::UpdateActiveTTY() {
  last_active_tty_ = active_tty_;

  active_tty_ = -1;
  char tty_string[8] = {0};
  if (tty_file_ && !fseek(tty_file_.get(), 0, SEEK_SET) &&
      fread(tty_string, 1, 7, tty_file_.get())) {
    int tty_number;
    if (sscanf(tty_string, "tty%d\n", &tty_number) == 1) {
      active_tty_ = tty_number;
    }
  }
}
#endif

bool GpuWatchdogThread::ContinueOnNonHostX11ServerTty() {
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)
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

}  // namespace gpu

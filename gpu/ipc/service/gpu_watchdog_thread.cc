// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_watchdog_thread.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/alias.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/power_monitor/power_monitor.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/config/gpu_crash_keys.h"
#include "ui/gl/shader_tracking.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace gpu {
namespace {

#if defined(CYGPROFILE_INSTRUMENTATION)
const int kGpuTimeout = 30000;
#elif defined(OS_WIN) || defined(OS_MACOSX)
// Use a slightly longer timeout on Windows due to prevalence of slow and
// infected machines.

// Also use a slightly longer timeout on MacOSX to get rid of GPU process
// hangs at context creation during startup. See https://crbug.com/918490.
const int kGpuTimeout = 15000;
#else
const int kGpuTimeout = 10000;
#endif

#if defined(USE_X11)
const base::FilePath::CharType kTtyFilePath[] =
    FILE_PATH_LITERAL("/sys/class/tty/tty0/active");
#endif

}  // namespace

GpuWatchdogThreadImplV1::GpuWatchdogThreadImplV1()
    : watched_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      timeout_(base::TimeDelta::FromMilliseconds(kGpuTimeout)),
      armed_(false),
      task_observer_(this),
      use_thread_cpu_time_(true),
      responsive_acknowledge_count_(0),
#if defined(OS_WIN)
      watched_thread_handle_(0),
      arm_cpu_time_(),
#endif
      suspension_counter_(this)
#if defined(USE_X11)
      ,
      host_tty_(-1)
#endif
{
  base::subtle::NoBarrier_Store(&awaiting_acknowledge_, false);

#if defined(OS_WIN)
  // GetCurrentThread returns a pseudo-handle that cannot be used by one thread
  // to identify another. DuplicateHandle creates a "real" handle that can be
  // used for this purpose.
  BOOL result = DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                                GetCurrentProcess(), &watched_thread_handle_,
                                THREAD_QUERY_INFORMATION, FALSE, 0);
  DCHECK(result);
#endif

#if defined(USE_X11)
  tty_file_ = base::OpenFile(base::FilePath(kTtyFilePath), "r");
  host_tty_ = GetActiveTTY();
#endif
  base::MessageLoopCurrent::Get()->AddTaskObserver(&task_observer_);
  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogStart);
}

// static
std::unique_ptr<GpuWatchdogThreadImplV1> GpuWatchdogThreadImplV1::Create(
    bool start_backgrounded) {
  auto watchdog_thread = base::WrapUnique(new GpuWatchdogThreadImplV1);
  base::Thread::Options options;
  options.timer_slack = base::TIMER_SLACK_MAXIMUM;
  watchdog_thread->StartWithOptions(options);
  if (start_backgrounded)
    watchdog_thread->OnBackgrounded();
  return watchdog_thread;
}

void GpuWatchdogThreadImplV1::CheckArmed() {
  base::subtle::NoBarrier_Store(&awaiting_acknowledge_, false);
}

void GpuWatchdogThreadImplV1::ReportProgress() {
  CheckArmed();
}

void GpuWatchdogThreadImplV1::OnBackgrounded() {
  // As we stop the task runner before destroying this class, the unretained
  // reference will always outlive the task.
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV1::OnBackgroundedOnWatchdogThread,
                     base::Unretained(this)));
}

void GpuWatchdogThreadImplV1::OnForegrounded() {
  // As we stop the task runner before destroying this class, the unretained
  // reference will always outlive the task.
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV1::OnForegroundedOnWatchdogThread,
                     base::Unretained(this)));
}

void GpuWatchdogThreadImplV1::GpuWatchdogHistogram(
    GpuWatchdogThreadEvent thread_event) {
  UMA_HISTOGRAM_ENUMERATION("GPU.WatchdogThread.Event", thread_event);
}

bool GpuWatchdogThreadImplV1::IsGpuHangDetectedForTesting() {
  return false;
}

void GpuWatchdogThreadImplV1::Init() {
  // Schedule the first check.
  OnCheck(false);
}

void GpuWatchdogThreadImplV1::CleanUp() {
  weak_factory_.InvalidateWeakPtrs();
  armed_ = false;
}

GpuWatchdogThreadImplV1::GpuWatchdogTaskObserver::GpuWatchdogTaskObserver(
    GpuWatchdogThreadImplV1* watchdog)
    : watchdog_(watchdog) {}

GpuWatchdogThreadImplV1::GpuWatchdogTaskObserver::~GpuWatchdogTaskObserver() =
    default;

void GpuWatchdogThreadImplV1::GpuWatchdogTaskObserver::WillProcessTask(
    const base::PendingTask& pending_task) {
  watchdog_->CheckArmed();
}

void GpuWatchdogThreadImplV1::GpuWatchdogTaskObserver::DidProcessTask(
    const base::PendingTask& pending_task) {}

GpuWatchdogThreadImplV1::SuspensionCounter::SuspensionCounterRef::
    SuspensionCounterRef(SuspensionCounter* counter)
    : counter_(counter) {
  counter_->OnAddRef();
}

GpuWatchdogThreadImplV1::SuspensionCounter::SuspensionCounterRef::
    ~SuspensionCounterRef() {
  counter_->OnReleaseRef();
}

GpuWatchdogThreadImplV1::SuspensionCounter::SuspensionCounter(
    GpuWatchdogThreadImplV1* watchdog_thread)
    : watchdog_thread_(watchdog_thread) {
  // This class will only be used on the watchdog thread, but is constructed on
  // the main thread. Detach.
  DETACH_FROM_SEQUENCE(watchdog_thread_sequence_checker_);
}

std::unique_ptr<
    GpuWatchdogThreadImplV1::SuspensionCounter::SuspensionCounterRef>
GpuWatchdogThreadImplV1::SuspensionCounter::Take() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watchdog_thread_sequence_checker_);
  return std::make_unique<SuspensionCounterRef>(this);
}

bool GpuWatchdogThreadImplV1::SuspensionCounter::HasRefs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watchdog_thread_sequence_checker_);
  return suspend_count_ > 0;
}

void GpuWatchdogThreadImplV1::SuspensionCounter::OnWatchdogThreadStopped() {
  DETACH_FROM_SEQUENCE(watchdog_thread_sequence_checker_);

  // Null the |watchdog_thread_| ptr at shutdown to avoid trying to suspend or
  // resume after the thread is stopped.
  watchdog_thread_ = nullptr;
}

void GpuWatchdogThreadImplV1::SuspensionCounter::OnAddRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watchdog_thread_sequence_checker_);
  suspend_count_++;
  if (watchdog_thread_ && suspend_count_ == 1)
    watchdog_thread_->SuspendStateChanged();
}

void GpuWatchdogThreadImplV1::SuspensionCounter::OnReleaseRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watchdog_thread_sequence_checker_);
  DCHECK_GT(suspend_count_, 0u);
  suspend_count_--;
  if (watchdog_thread_ && suspend_count_ == 0)
    watchdog_thread_->SuspendStateChanged();
}

GpuWatchdogThreadImplV1::~GpuWatchdogThreadImplV1() {
  DCHECK(watched_task_runner_->BelongsToCurrentThread());

  Stop();
  suspension_counter_.OnWatchdogThreadStopped();

#if defined(OS_WIN)
  CloseHandle(watched_thread_handle_);
#endif

  base::PowerMonitor::RemoveObserver(this);

#if defined(USE_X11)
  if (tty_file_)
    fclose(tty_file_);
#endif

  base::MessageLoopCurrent::Get()->RemoveTaskObserver(&task_observer_);
  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogEnd);
}

void GpuWatchdogThreadImplV1::OnAcknowledge() {
  CHECK(base::PlatformThread::CurrentId() == GetThreadId());

  // The check has already been acknowledged and another has already been
  // scheduled by a previous call to OnAcknowledge. It is normal for a
  // watched thread to see armed_ being true multiple times before
  // the OnAcknowledge task is run on the watchdog thread.
  if (!armed_)
    return;

  // Revoke any pending hang termination.
  weak_factory_.InvalidateWeakPtrs();
  armed_ = false;

  if (suspension_counter_.HasRefs()) {
    responsive_acknowledge_count_ = 0;
    return;
  }

  base::Time current_time = base::Time::Now();

  // The watchdog waits until at least 6 consecutive checks have returned in
  // less than 50 ms before it will start ignoring the CPU time in determining
  // whether to timeout. This is a compromise to allow startups that are slow
  // due to disk contention to avoid timing out, but once the GPU process is
  // running smoothly the watchdog will be able to detect hangs that don't use
  // the CPU.
  if ((current_time - check_time_) < base::TimeDelta::FromMilliseconds(50))
    responsive_acknowledge_count_++;
  else
    responsive_acknowledge_count_ = 0;

  if (responsive_acknowledge_count_ >= 6)
    use_thread_cpu_time_ = false;

  // If it took a long time for the acknowledgement, assume the computer was
  // recently suspended.
  bool was_suspended = (current_time > suspension_timeout_);

  // The monitored thread has responded. Post a task to check it again.
  task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV1::OnCheck,
                     weak_factory_.GetWeakPtr(), was_suspended),
      0.5 * timeout_);
}

void GpuWatchdogThreadImplV1::OnCheck(bool after_suspend) {
  CHECK(base::PlatformThread::CurrentId() == GetThreadId());

  // Do not create any new termination tasks if one has already been created
  // or the system is suspended.
  if (armed_ || suspension_counter_.HasRefs())
    return;

  armed_ = true;

  // Must set |awaiting_acknowledge_| before posting the task.  This task might
  // be the only task that will activate the TaskObserver on the watched thread
  // and it must not miss the false -> true transition. No barrier is needed
  // here, as the PostTask which follows contains a barrier.
  base::subtle::NoBarrier_Store(&awaiting_acknowledge_, true);

#if defined(OS_WIN)
  arm_cpu_time_ = GetWatchedThreadTime();

  QueryUnbiasedInterruptTime(&arm_interrupt_time_);
#endif

  check_time_ = base::Time::Now();
  check_timeticks_ = base::TimeTicks::Now();
  // Immediately after the computer is woken up from being suspended it might
  // be pretty sluggish, so allow some extra time before the next timeout.
  base::TimeDelta timeout = timeout_ * (after_suspend ? 3 : 1);
  suspension_timeout_ = check_time_ + timeout * 2;

  // Post a task to the monitored thread that does nothing but wake up the
  // TaskObserver. Any other tasks that are pending on the watched thread will
  // also wake up the observer. This simply ensures there is at least one.
  watched_task_runner_->PostTask(FROM_HERE, base::DoNothing());

  // Post a task to the watchdog thread to exit if the monitored thread does
  // not respond in time.
  task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV1::OnCheckTimeout,
                     weak_factory_.GetWeakPtr()),
      timeout);
}

void GpuWatchdogThreadImplV1::OnCheckTimeout() {
  DeliberatelyTerminateToRecoverFromHang();
}

// Use the --disable-gpu-watchdog command line switch to disable this.
void GpuWatchdogThreadImplV1::DeliberatelyTerminateToRecoverFromHang() {
  // Should not get here while the system is suspended.
  DCHECK(!suspension_counter_.HasRefs());

  // If the watchdog woke up significantly behind schedule, disarm and reset
  // the watchdog check. This is to prevent the watchdog thread from terminating
  // when a machine wakes up from sleep or hibernation, which would otherwise
  // appear to be a hang.
  if (base::Time::Now() > suspension_timeout_) {
    OnAcknowledge();
    return;
  }

  if (!base::subtle::NoBarrier_Load(&awaiting_acknowledge_)) {
    OnAcknowledge();
    return;
  }

#if defined(OS_WIN)
  // Defer termination until a certain amount of CPU time has elapsed on the
  // watched thread.
  base::ThreadTicks current_cpu_time = GetWatchedThreadTime();
  base::TimeDelta time_since_arm = current_cpu_time - arm_cpu_time_;
  if (use_thread_cpu_time_ && (time_since_arm < timeout_)) {
    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GpuWatchdogThreadImplV1::OnCheckTimeout,
                       weak_factory_.GetWeakPtr()),
        timeout_ - time_since_arm);
    return;
  }
#endif

  // For minimal developer annoyance, don't keep terminating. You need to skip
  // the call to base::Process::Terminate below in a debugger for this to be
  // useful.
  static bool terminated = false;
  if (terminated)
    return;

#if defined(OS_WIN)
  if (IsDebuggerPresent())
    return;
#endif

#if defined(USE_X11)
  // Don't crash if we're not on the TTY of our host X11 server.
  int active_tty = GetActiveTTY();
  if (host_tty_ != -1 && active_tty != -1 && host_tty_ != active_tty) {
    return;
  }
#endif

// Store variables so they're available in crash dumps to help determine the
// cause of any hang.
#if defined(OS_WIN)
  ULONGLONG fire_interrupt_time;
  QueryUnbiasedInterruptTime(&fire_interrupt_time);

  // This is the time since the watchdog was armed, in 100ns intervals,
  // ignoring time where the computer is suspended.
  ULONGLONG interrupt_delay = fire_interrupt_time - arm_interrupt_time_;

  base::debug::Alias(&interrupt_delay);
  base::debug::Alias(&current_cpu_time);
  base::debug::Alias(&time_since_arm);

  bool using_thread_ticks = base::ThreadTicks::IsSupported();
  base::debug::Alias(&using_thread_ticks);

  bool using_high_res_timer = base::TimeTicks::IsHighResolution();
  base::debug::Alias(&using_high_res_timer);
#endif

  int32_t awaiting_acknowledge =
      base::subtle::NoBarrier_Load(&awaiting_acknowledge_);
  base::debug::Alias(&awaiting_acknowledge);

  // Don't log the message to stderr in release builds because the buffer
  // may be full.
  std::string message = base::StringPrintf(
      "The GPU process hung. Terminating after %" PRId64 " ms.",
      timeout_.InMilliseconds());
  logging::LogMessageHandlerFunction handler = logging::GetLogMessageHandler();
  if (handler)
    handler(logging::LOG_ERROR, __FILE__, __LINE__, 0, message);
  DLOG(ERROR) << message;

  base::Time current_time = base::Time::Now();
  base::TimeTicks current_timeticks = base::TimeTicks::Now();
  base::debug::Alias(&current_time);
  base::debug::Alias(&current_timeticks);

  int64_t available_physical_memory =
      base::SysInfo::AmountOfAvailablePhysicalMemory() >> 20;
  crash_keys::available_physical_memory_in_mb.Set(
      base::NumberToString(available_physical_memory));

  gl::ShaderTracking* shader_tracking = gl::ShaderTracking::GetInstance();
  if (shader_tracking) {
    std::string shaders[2];
    shader_tracking->GetShaders(shaders, shaders + 1);
    crash_keys::current_shader_0.Set(shaders[0]);
    crash_keys::current_shader_1.Set(shaders[1]);
  }

  // Check it one last time before crashing.
  if (!base::subtle::NoBarrier_Load(&awaiting_acknowledge_)) {
    OnAcknowledge();
    return;
  }

  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogKill);

  // Deliberately crash the process to create a crash dump.
  *((volatile int*)0) = 0x1337;

  terminated = true;
}

void GpuWatchdogThreadImplV1::AddPowerObserver() {
  // As we stop the task runner before destroying this class, the unretained
  // reference will always outlive the task.
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuWatchdogThreadImplV1::OnAddPowerObserver,
                                base::Unretained(this)));
}

void GpuWatchdogThreadImplV1::OnAddPowerObserver() {
  DCHECK(base::PowerMonitor::IsInitialized());
  base::PowerMonitor::AddObserver(this);
}

void GpuWatchdogThreadImplV1::OnSuspend() {
  power_suspend_ref_ = suspension_counter_.Take();
}

void GpuWatchdogThreadImplV1::OnResume() {
  power_suspend_ref_.reset();
}

void GpuWatchdogThreadImplV1::OnBackgroundedOnWatchdogThread() {
  background_suspend_ref_ = suspension_counter_.Take();
}

void GpuWatchdogThreadImplV1::OnForegroundedOnWatchdogThread() {
  background_suspend_ref_.reset();
}

void GpuWatchdogThreadImplV1::SuspendStateChanged() {
  if (suspension_counter_.HasRefs()) {
    suspend_time_ = base::Time::Now();
    // When suspending force an acknowledgement to cancel any pending
    // termination tasks.
    OnAcknowledge();
  } else {
    resume_time_ = base::Time::Now();

    // After resuming jump-start the watchdog again.
    armed_ = false;
    OnCheck(true);
  }
}

#if defined(OS_WIN)
base::ThreadTicks GpuWatchdogThreadImplV1::GetWatchedThreadTime() {
  if (base::ThreadTicks::IsSupported()) {
    // Convert ThreadTicks::Now() to TimeDelta.
    return base::ThreadTicks::GetForThread(
        base::PlatformThreadHandle(watched_thread_handle_));
  } else {
    // Use GetThreadTimes as a backup mechanism.
    FILETIME creation_time;
    FILETIME exit_time;
    FILETIME user_time;
    FILETIME kernel_time;
    BOOL result = GetThreadTimes(watched_thread_handle_, &creation_time,
                                 &exit_time, &kernel_time, &user_time);
    DCHECK(result);

    ULARGE_INTEGER user_time64;
    user_time64.HighPart = user_time.dwHighDateTime;
    user_time64.LowPart = user_time.dwLowDateTime;

    ULARGE_INTEGER kernel_time64;
    kernel_time64.HighPart = kernel_time.dwHighDateTime;
    kernel_time64.LowPart = kernel_time.dwLowDateTime;

    // Time is reported in units of 100 nanoseconds. Kernel and user time are
    // summed to deal with to kinds of hangs. One is where the GPU process is
    // stuck in user level, never calling into the kernel and kernel time is
    // not increasing. The other is where either the kernel hangs and never
    // returns to user level or where user level code
    // calls into kernel level repeatedly, giving up its quanta before it is
    // tracked, for example a loop that repeatedly Sleeps.
    return base::ThreadTicks() +
           base::TimeDelta::FromMilliseconds(static_cast<int64_t>(
               (user_time64.QuadPart + kernel_time64.QuadPart) / 10000));
  }
}
#endif

#if defined(USE_X11)
int GpuWatchdogThreadImplV1::GetActiveTTY() const {
  char tty_string[8] = {0};
  if (tty_file_ && !fseek(tty_file_, 0, SEEK_SET) &&
      fread(tty_string, 1, 7, tty_file_)) {
    int tty_number;
    size_t num_res = sscanf(tty_string, "tty%d\n", &tty_number);
    if (num_res == 1)
      return tty_number;
  }
  return -1;
}
#endif

GpuWatchdogThread::GpuWatchdogThread() : base::Thread("GpuWatchdog") {}
GpuWatchdogThread::~GpuWatchdogThread() {}

}  // namespace gpu

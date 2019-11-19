// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_watchdog_thread_v2.h"

#if defined(OS_WIN)
#include <d3d9.h>
#include <dxgi.h>
#endif

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/bit_cast.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop_current.h"
#include "base/native_library.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "gpu/config/gpu_crash_keys.h"

namespace gpu {
namespace {

// For crash_keys::gpu_watchdog_slow_system_type
enum GpuWatchdogCrashType {
  kGpuHang = 0,
  kSlow = 1,
  kSlowFileAccess = 2,
  kSlowThreads = 3,
};

#if defined(OS_WIN)
bool LoadAndUnloadAFile(
    base::NativeLibraryLoadError* native_library_load_error) {
  // Try loading d3d9.dll first
  base::NativeLibrary d3d9_library = base::LoadNativeLibrary(
      base::FilePath(L"d3d9.dll"), native_library_load_error);
  if (d3d9_library) {
    typedef IDirect3D9Ex*(__stdcall * PFNDIRECT3DCREATE9EX)(unsigned int,
                                                            IDirect3D9Ex**);
    PFNDIRECT3DCREATE9EX direct3d_create9ex;
    direct3d_create9ex =
        (PFNDIRECT3DCREATE9EX)GetProcAddress(d3d9_library, "Direct3DCreate9Ex");

    base::UnloadNativeLibrary(d3d9_library);
    return true;
  }

  // if d3d9.dll is not available, try dxgi.dll
  base::NativeLibrary dxgi_library = base::LoadNativeLibrary(
      base::FilePath(L"dxgi.dll"), native_library_load_error);
  if (dxgi_library) {
    typedef HRESULT(WINAPI * PFNCREATEDXGIFACTORY1)(REFIID riid,
                                                    void** ppFactory);
    PFNCREATEDXGIFACTORY1 create_dxgi_factory1;
    create_dxgi_factory1 = (PFNCREATEDXGIFACTORY1)GetProcAddress(
        dxgi_library, "CreateDXGIFactory1");

    base::UnloadNativeLibrary(dxgi_library);
    return true;
  }

  return false;
}

bool CreateAndDestroyAThread() {
  auto thread = base::WrapUnique(new base::Thread("GpuWatchdogKill"));
  base::Thread::Options options;
  // BACKGROUND: for threads that shouldn't disrupt high priority work.
  options.priority = base::ThreadPriority::BACKGROUND;
  options.timer_slack = base::TIMER_SLACK_MAXIMUM;
  bool successful = thread->StartWithOptions(options);

  if (successful) {
    successful = thread->WaitUntilThreadStarted();
    thread->Stop();
  }
  return successful;
}
#endif
}  // namespace

GpuWatchdogThreadImplV2::GpuWatchdogThreadImplV2(base::TimeDelta timeout,
                                                 bool is_test_mode)
    : watchdog_timeout_(timeout),
      is_test_mode_(is_test_mode),
      watched_gpu_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  base::MessageLoopCurrent::Get()->AddTaskObserver(this);
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

  Arm();
}

GpuWatchdogThreadImplV2::~GpuWatchdogThreadImplV2() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());
  Stop();  // stop the watchdog thread

  base::MessageLoopCurrent::Get()->RemoveTaskObserver(this);
  base::PowerMonitor::RemoveObserver(this);
  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogEnd);
#if defined(OS_WIN)
  if (watched_thread_handle_)
    CloseHandle(watched_thread_handle_);
#endif
}

// static
std::unique_ptr<GpuWatchdogThreadImplV2> GpuWatchdogThreadImplV2::Create(
    bool start_backgrounded,
    base::TimeDelta timeout,
    bool is_test_mode) {
  auto watchdog_thread =
      base::WrapUnique(new GpuWatchdogThreadImplV2(timeout, is_test_mode));
  base::Thread::Options options;
  options.timer_slack = base::TIMER_SLACK_MAXIMUM;
  watchdog_thread->StartWithOptions(options);
  if (start_backgrounded)
    watchdog_thread->OnBackgrounded();
  return watchdog_thread;
}

// static
std::unique_ptr<GpuWatchdogThreadImplV2> GpuWatchdogThreadImplV2::Create(
    bool start_backgrounded) {
  return Create(start_backgrounded, kGpuWatchdogTimeout, false);
}

// Do not add power observer during watchdog init, PowerMonitor might not be up
// running yet.
void GpuWatchdogThreadImplV2::AddPowerObserver() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  // Forward it to the watchdog thread. Call PowerMonitor::AddObserver on the
  // watchdog thread so that OnSuspend and OnResume will be called on watchdog
  // thread.
  is_add_power_observer_called_ = true;
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuWatchdogThreadImplV2::OnAddPowerObserver,
                                base::Unretained(this)));
}

// Called from the gpu thread.
void GpuWatchdogThreadImplV2::OnBackgrounded() {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogBackgrounded,
                     base::Unretained(this)));
}

// Called from the gpu thread.
void GpuWatchdogThreadImplV2::OnForegrounded() {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogForegrounded,
                     base::Unretained(this)));
}

// Called from the gpu thread when gpu init has completed.
void GpuWatchdogThreadImplV2::OnInitComplete() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());
  Disarm();
}

// Called from the gpu thread in viz::GpuServiceImpl::~GpuServiceImpl().
// After this, no Disarm() will be called before the watchdog thread is
// destroyed. If this destruction takes too long, the watchdog timeout
// will be triggered.
void GpuWatchdogThreadImplV2::OnGpuProcessTearDown() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  in_gpu_process_teardown_ = true;
  if (!IsArmed())
    Arm();
}

// Running on the watchdog thread.
// On Linux, Init() will be called twice for Sandbox Initialization. The
// watchdog is stopped and then restarted in StartSandboxLinux(). Everything
// should be the same and continue after the second init().
void GpuWatchdogThreadImplV2::Init() {
  watchdog_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  // Get and Invalidate weak_ptr should be done on the watchdog thread only.
  weak_ptr_ = weak_factory_.GetWeakPtr();
  base::TimeDelta timeout = watchdog_timeout_ * kInitFactor;
  task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogTimeout, weak_ptr_),
      timeout);

  last_arm_disarm_counter_ = base::subtle::NoBarrier_Load(&arm_disarm_counter_);
  watchdog_start_timeticks_ = base::TimeTicks::Now();
  last_on_watchdog_timeout_timeticks_ = watchdog_start_timeticks_;
#if defined(OS_WIN)
  if (watched_thread_handle_) {
    if (base::ThreadTicks::IsSupported())
      base::ThreadTicks::WaitUntilInitialized();
    last_on_watchdog_timeout_thread_ticks_ = GetWatchedThreadTime();
    remaining_watched_thread_ticks_ = timeout;
  }
#endif

  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogStart);
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::CleanUp() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  weak_factory_.InvalidateWeakPtrs();
}

void GpuWatchdogThreadImplV2::ReportProgress() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());
  InProgress();
}

void GpuWatchdogThreadImplV2::WillProcessTask(
    const base::PendingTask& pending_task) {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  // The watchdog is armed at the beginning of the gpu process teardown.
  // Do not call Arm() during teardown.
  if (in_gpu_process_teardown_)
    DCHECK(IsArmed());
  else
    Arm();
}

void GpuWatchdogThreadImplV2::DidProcessTask(
    const base::PendingTask& pending_task) {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  // Keep the watchdog armed during tear down.
  if (in_gpu_process_teardown_)
    InProgress();
  else
    Disarm();
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnSuspend() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  in_power_suspension_ = true;
  // Revoke any pending watchdog timeout task
  weak_factory_.InvalidateWeakPtrs();
  suspend_timeticks_ = base::TimeTicks::Now();
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnResume() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());

  in_power_suspension_ = false;
  RestartWatchdogTimeoutTask();
  resume_timeticks_ = base::TimeTicks::Now();
  is_first_timeout_after_power_resume = true;
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnAddPowerObserver() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(base::PowerMonitor::IsInitialized());

  is_power_observer_added_ = base::PowerMonitor::AddObserver(this);
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnWatchdogBackgrounded() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());

  is_backgrounded_ = true;
  // Revoke any pending watchdog timeout task
  weak_factory_.InvalidateWeakPtrs();
  backgrounded_timeticks_ = base::TimeTicks::Now();
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnWatchdogForegrounded() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());

  is_backgrounded_ = false;
  RestartWatchdogTimeoutTask();
  foregrounded_timeticks_ = base::TimeTicks::Now();
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::RestartWatchdogTimeoutTask() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());

  if (!is_backgrounded_ && !in_power_suspension_) {
    // Make the timeout twice long. The system/gpu might be very slow right
    // after resume or foregrounded.
    weak_ptr_ = weak_factory_.GetWeakPtr();
    base::TimeDelta timeout = watchdog_timeout_ * kRestartFactor;
    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogTimeout, weak_ptr_),
        timeout);
    last_on_watchdog_timeout_timeticks_ = base::TimeTicks::Now();
#if defined(OS_WIN)
    if (watched_thread_handle_) {
      last_on_watchdog_timeout_thread_ticks_ = GetWatchedThreadTime();
      remaining_watched_thread_ticks_ = timeout;
    }
#endif
  }
}

// Called from the gpu main thread.
// The watchdog is armed only in these three functions -
// GpuWatchdogThreadImplV2(), WillProcessTask(), and OnGpuProcessTearDown()
void GpuWatchdogThreadImplV2::Arm() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  base::subtle::NoBarrier_AtomicIncrement(&arm_disarm_counter_, 1);

  // Arm/Disarm are always called in sequence. Now it's an odd number.
  DCHECK(IsArmed());
}

void GpuWatchdogThreadImplV2::Disarm() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  base::subtle::NoBarrier_AtomicIncrement(&arm_disarm_counter_, 1);

  // Arm/Disarm are always called in sequence. Now it's an even number.
  DCHECK(!IsArmed());
}

void GpuWatchdogThreadImplV2::InProgress() {
  DCHECK(watched_gpu_task_runner_->BelongsToCurrentThread());

  // Increment by 2. This is equivalent to Disarm() + Arm().
  base::subtle::NoBarrier_AtomicIncrement(&arm_disarm_counter_, 2);

  // Now it's an odd number.
  DCHECK(IsArmed());
}

bool GpuWatchdogThreadImplV2::IsArmed() {
  // It's an odd number.
  return base::subtle::NoBarrier_Load(&arm_disarm_counter_) & 1;
}

// Running on the watchdog thread.
void GpuWatchdogThreadImplV2::OnWatchdogTimeout() {
  DCHECK(watchdog_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!is_backgrounded_);
  DCHECK(!in_power_suspension_);
  base::subtle::Atomic32 arm_disarm_counter =
      base::subtle::NoBarrier_Load(&arm_disarm_counter_);

  // Collect all needed info for gpu hang detection.
  bool disarmed = arm_disarm_counter % 2 == 0;  // even number
  bool gpu_makes_progress = arm_disarm_counter != last_arm_disarm_counter_;
  last_arm_disarm_counter_ = arm_disarm_counter;
  bool gpu_thread_needs_more_time =
      WatchedThreadNeedsMoreTime(disarmed || gpu_makes_progress);

  // No gpu hang is detected. Continue with another OnWatchdogTimeout task
  if (disarmed || gpu_makes_progress || gpu_thread_needs_more_time) {
    last_on_watchdog_timeout_timeticks_ = base::TimeTicks::Now();
    is_first_timeout_after_power_resume = false;

    task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GpuWatchdogThreadImplV2::OnWatchdogTimeout, weak_ptr_),
        watchdog_timeout_);
    return;
  }

  // Still armed without any progress. GPU possibly hangs.
  DeliberatelyTerminateToRecoverFromHang();
}

bool GpuWatchdogThreadImplV2::GpuIsAlive() {
  base::subtle::Atomic32 arm_disarm_counter =
      base::subtle::NoBarrier_Load(&arm_disarm_counter_);
  bool gpu_makes_progress = arm_disarm_counter != last_arm_disarm_counter_;

  return (gpu_makes_progress);
}

bool GpuWatchdogThreadImplV2::WatchedThreadNeedsMoreTime(
    bool no_gpu_hang_detected) {
#if defined(OS_WIN)
  if (!watched_thread_handle_)
    return false;

  base::ThreadTicks now = GetWatchedThreadTime();
  base::TimeDelta thread_time_elapsed =
      now - last_on_watchdog_timeout_thread_ticks_;
  last_on_watchdog_timeout_thread_ticks_ = now;
  remaining_watched_thread_ticks_ -= thread_time_elapsed;

  if (no_gpu_hang_detected ||
      count_of_more_gpu_thread_time_allowed >=
          kMaxCountOfMoreGpuThreadTimeAllowed ||
      thread_time_elapsed < base::TimeDelta() /* bogus data */ ||
      remaining_watched_thread_ticks_ <= base::TimeDelta()) {
    // Reset the remaining thread ticks.
    remaining_watched_thread_ticks_ = watchdog_timeout_;
    count_of_more_gpu_thread_time_allowed = 0;
    return false;
  } else {
    count_of_more_gpu_thread_time_allowed++;
    return true;
  }
#else
  return false;
#endif
}

#if defined(OS_WIN)
base::ThreadTicks GpuWatchdogThreadImplV2::GetWatchedThreadTime() {
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

void GpuWatchdogThreadImplV2::DeliberatelyTerminateToRecoverFromHang() {
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
  bool in_gpu_init = base::subtle::NoBarrier_Load(&arm_disarm_counter_) == 1;
  base::TimeTicks function_begin_timeticks = base::TimeTicks::Now();
  base::debug::Alias(&in_gpu_init);
  base::debug::Alias(&function_begin_timeticks);
  base::debug::Alias(&watchdog_start_timeticks_);
  base::debug::Alias(&suspend_timeticks_);
  base::debug::Alias(&resume_timeticks_);
  base::debug::Alias(&backgrounded_timeticks_);
  base::debug::Alias(&foregrounded_timeticks_);
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
#endif

  GpuWatchdogHistogram(GpuWatchdogThreadEvent::kGpuWatchdogKill);

#if defined(OS_WIN)
  base::TimeDelta extra_time_to_survive;
  int crash_key_type = kGpuHang;

  if (GpuIsAlive()) {
    extra_time_to_survive = base::TimeTicks::Now() - function_begin_timeticks;
    crash_key_type = kSlow;
  }

  // (1)  Check how long it takes to load and unload a library from disk.
  base::TimeTicks loading_start = base::TimeTicks::Now();
  base::NativeLibraryLoadError native_library_load_error;
  bool loading_successful = false;
  base::TimeDelta file_loading_total;

  // See if the gpu makes any progress after loading a file from disk
  if (crash_key_type == kGpuHang) {
    loading_successful = LoadAndUnloadAFile(&native_library_load_error);
    file_loading_total = base::TimeTicks::Now() - loading_start;

    if (GpuIsAlive()) {
      extra_time_to_survive = base::TimeTicks::Now() - function_begin_timeticks;
      if (loading_successful)
        crash_key_type = kSlowFileAccess;
      else
        crash_key_type = kSlow;
    }
  }

  base::debug::Alias(&loading_start);
  base::debug::Alias(&native_library_load_error);
  base::debug::Alias(&loading_successful);
  base::debug::Alias(&file_loading_total);

  // (2) Check how long it takes to create and destroy a new thread.
  bool thread_successful = false;
  base::TimeTicks thread_start = base::TimeTicks::Now();
  base::TimeDelta thread_total;

  if (crash_key_type == kGpuHang) {
    thread_successful = CreateAndDestroyAThread();
    thread_total = base::TimeTicks::Now() - thread_start;

    // See if the gpu makes any progress after creating a new thread.
    if (GpuIsAlive()) {
      extra_time_to_survive = base::TimeTicks::Now() - function_begin_timeticks;
      if (thread_successful)
        crash_key_type = kSlowThreads;
      else
        crash_key_type = kSlow;
    }
  }
  base::debug::Alias(&thread_successful);
  base::debug::Alias(&thread_total);

  // (3) Continue waiting until 60 seconds has passed in this funtion.
  base::TimeTicks wait_start = base::TimeTicks::Now();
  base::debug::Alias(&wait_start);

  if (crash_key_type == kGpuHang) {
    while (base::TimeTicks::Now() - function_begin_timeticks <
           base::TimeDelta::FromSeconds(60)) {
      // Sleep for 5 seconds each time and check if the GPU has made a progress.
      base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(5));

      if (GpuIsAlive()) {
        extra_time_to_survive =
            base::TimeTicks::Now() - function_begin_timeticks;
        crash_key_type = kSlow;
        break;
      }
    }
  }

  base::debug::Alias(&crash_key_type);
  base::debug::Alias(&extra_time_to_survive);

  crash_keys::gpu_watchdog_crashed_in_gpu_init.Set(in_gpu_init ? "1" : "0");
  crash_keys::gpu_watchdog_slow_system_type.Set(
      base::NumberToString(crash_key_type));
  crash_keys::gpu_watchdog_extra_seconds_needed.Set(
      base::NumberToString(extra_time_to_survive.InSeconds()));
#endif

  crash_keys::gpu_watchdog_kill_after_power_resume.Set(
      is_first_timeout_after_power_resume ? "1" : "0");

  // Deliberately crash the process to create a crash dump.
  *((volatile int*)0) = 0xdeadface;
}

void GpuWatchdogThreadImplV2::GpuWatchdogHistogram(
    GpuWatchdogThreadEvent thread_event) {
  UMA_HISTOGRAM_ENUMERATION("GPU.WatchdogThread.Event.V2", thread_event);
  UMA_HISTOGRAM_ENUMERATION("GPU.WatchdogThread.Event", thread_event);
}

// For gpu testing only. Return whether a GPU hang was detected or not.
bool GpuWatchdogThreadImplV2::IsGpuHangDetectedForTesting() {
  DCHECK(is_test_mode_);
  return test_result_timeout_and_gpu_hang_.IsSet();
}

// This should be called on the test main thread only. It will wait until the
// power observer is added on the watchdog thread.
void GpuWatchdogThreadImplV2::WaitForPowerObserverAddedForTesting() {
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

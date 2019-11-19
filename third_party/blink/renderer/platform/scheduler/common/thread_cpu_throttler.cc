// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/thread_cpu_throttler.h"

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/synchronization/atomic_flag.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include <signal.h>
#define USE_SIGNALS 1
#elif defined(OS_WIN)
#include <windows.h>
#endif

using base::subtle::Atomic32;
using base::subtle::Acquire_Load;
using base::subtle::Release_Store;

namespace blink {
namespace scheduler {

class ThreadCPUThrottler::ThrottlingThread final
    : public base::PlatformThread::Delegate {
 public:
  explicit ThrottlingThread(double rate);
  ~ThrottlingThread() override;

  void SetThrottlingRate(double rate);

 private:
  void ThreadMain() override;

  void Start();
  void Stop();
  void Throttle();

  static void SuspendThread(base::PlatformThreadHandle thread_handle);
  static void ResumeThread(base::PlatformThreadHandle thread_handle);
  static void Sleep(base::TimeDelta duration);

#ifdef USE_SIGNALS
  void InstallSignalHandler();
  void RestoreSignalHandler();
  static void HandleSignal(int signal);

  static bool signal_handler_installed_;
  static struct sigaction old_signal_handler_;
#endif
  static Atomic32 thread_exists_;
  static Atomic32 throttling_rate_percent_;

  base::PlatformThreadHandle throttled_thread_handle_;
  base::PlatformThreadHandle throttling_thread_handle_;
  base::AtomicFlag cancellation_flag_;

  DISALLOW_COPY_AND_ASSIGN(ThrottlingThread);
};

#ifdef USE_SIGNALS
bool ThreadCPUThrottler::ThrottlingThread::signal_handler_installed_;
struct sigaction ThreadCPUThrottler::ThrottlingThread::old_signal_handler_;
#endif
Atomic32 ThreadCPUThrottler::ThrottlingThread::throttling_rate_percent_;
Atomic32 ThreadCPUThrottler::ThrottlingThread::thread_exists_;

ThreadCPUThrottler::ThrottlingThread::ThrottlingThread(double rate)
#ifdef OS_WIN
    : throttled_thread_handle_(
          ::OpenThread(THREAD_SUSPEND_RESUME, false, ::GetCurrentThreadId())) {
#else
    : throttled_thread_handle_(base::PlatformThread::CurrentHandle()) {
#endif
  SetThrottlingRate(rate);
  CHECK_EQ(base::subtle::NoBarrier_AtomicExchange(&thread_exists_, 1), 0);
  Start();
}  // namespace scheduler

ThreadCPUThrottler::ThrottlingThread::~ThrottlingThread() {
  Stop();
  CHECK_EQ(base::subtle::NoBarrier_AtomicExchange(&thread_exists_, 0), 1);
}

void ThreadCPUThrottler::ThrottlingThread::SetThrottlingRate(double rate) {
  Release_Store(&throttling_rate_percent_, static_cast<Atomic32>(rate * 100));
}

void ThreadCPUThrottler::ThrottlingThread::ThreadMain() {
  base::PlatformThread::SetName("CPUThrottlingThread");
  while (!cancellation_flag_.IsSet()) {
    Throttle();
  }
}

#ifdef USE_SIGNALS

// static
void ThreadCPUThrottler::ThrottlingThread::InstallSignalHandler() {
  // There must be the only one!
  DCHECK(!signal_handler_installed_);
  struct sigaction sa;
  sa.sa_handler = &HandleSignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  signal_handler_installed_ =
      (sigaction(SIGUSR2, &sa, &old_signal_handler_) == 0);
}

// static
void ThreadCPUThrottler::ThrottlingThread::RestoreSignalHandler() {
  if (!signal_handler_installed_)
    return;
  sigaction(SIGUSR2, &old_signal_handler_, nullptr);
  signal_handler_installed_ = false;
}

// static
void ThreadCPUThrottler::ThrottlingThread::HandleSignal(int signal) {
  if (signal != SIGUSR2)
    return;
  static base::TimeTicks lastResumeTime;
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta run_duration = now - lastResumeTime;
  uint32_t throttling_rate_percent = Acquire_Load(&throttling_rate_percent_);
  // Limit the observed run duration to 1000μs to deal with the first entrance
  // to the signal handler.
  uint32_t run_duration_us = static_cast<uint32_t>(
      std::min(run_duration.InMicroseconds(), static_cast<int64_t>(1000)));
  uint32_t sleep_duration_us =
      run_duration_us * throttling_rate_percent / 100 - run_duration_us;
  base::TimeTicks wake_up_time =
      now + base::TimeDelta::FromMicroseconds(sleep_duration_us);
  do {
    now = base::TimeTicks::Now();
  } while (now < wake_up_time);
  lastResumeTime = now;
}

#endif  // USE_SIGNALS

void ThreadCPUThrottler::ThrottlingThread::Throttle() {
  const int quant_time_us = 200;
#ifdef USE_SIGNALS
  pthread_kill(throttled_thread_handle_.platform_handle(), SIGUSR2);
  Sleep(base::TimeDelta::FromMicroseconds(quant_time_us));
#elif defined(OS_WIN)
  double rate = Acquire_Load(&throttling_rate_percent_) / 100.;
  base::TimeDelta run_duration =
      base::TimeDelta::FromMicroseconds(static_cast<int>(quant_time_us / rate));
  base::TimeDelta sleep_duration =
      base::TimeDelta::FromMicroseconds(quant_time_us) - run_duration;
  Sleep(run_duration);
  ::SuspendThread(throttled_thread_handle_.platform_handle());
  Sleep(sleep_duration);
  ::ResumeThread(throttled_thread_handle_.platform_handle());
#else
  ALLOW_UNUSED_LOCAL(quant_time_us);
#endif
}

void ThreadCPUThrottler::ThrottlingThread::Start() {
#ifdef USE_SIGNALS
  InstallSignalHandler();
#elif !defined(OS_WIN)
  LOG(ERROR) << "CPU throttling is not supported.";
  return;
#endif
  if (!base::PlatformThread::Create(0, this, &throttling_thread_handle_)) {
    LOG(ERROR) << "Failed to create throttling thread.";
  }
}

void ThreadCPUThrottler::ThrottlingThread::Sleep(base::TimeDelta duration) {
#if defined(OS_WIN)
  // We cannot rely on ::Sleep function as it's precision is not enough for
  // the purpose. Could be up to 16ms jitter.
  base::TimeTicks wakeup_time = base::TimeTicks::Now() + duration;
  while (base::TimeTicks::Now() < wakeup_time) {
  }
#else
  base::PlatformThread::Sleep(duration);
#endif
}

void ThreadCPUThrottler::ThrottlingThread::Stop() {
  cancellation_flag_.Set();
  base::PlatformThread::Join(throttling_thread_handle_);
#ifdef USE_SIGNALS
  RestoreSignalHandler();
#endif
}

ThreadCPUThrottler::ThreadCPUThrottler() = default;
ThreadCPUThrottler::~ThreadCPUThrottler() = default;

void ThreadCPUThrottler::SetThrottlingRate(double rate) {
  if (rate <= 1) {
    if (throttling_thread_) {
      throttling_thread_.reset();
    }
    return;
  }
  if (throttling_thread_) {
    throttling_thread_->SetThrottlingRate(rate);
  } else {
    throttling_thread_.reset(new ThrottlingThread(rate));
  }
}

// static
ThreadCPUThrottler* ThreadCPUThrottler::GetInstance() {
  return base::Singleton<ThreadCPUThrottler>::get();
}

}  // namespace scheduler
}  // namespace blink

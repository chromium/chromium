// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/cpu_affinity.h"
#include "content/public/common/cpu_affinity.h"

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/cpu_affinity_posix.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/process/internal_linux.h"
#include "base/process/process_handle.h"
#include "base/task/current_thread.h"
#include "base/task/task_observer.h"
#include "base/threading/thread_task_runner_handle.h"

namespace {

void ApplyProcessCpuAffinityMode(base::CpuAffinityMode mode) {
  // Restrict affinity of all existing threads of the current process. The
  // affinity is inherited by any subsequently created thread. Other threads may
  // already exist even during early startup (e.g. Java threads like
  // RenderThread), so setting the affinity only for the current thread is not
  // enough here.
  bool success =
      base::SetProcessCpuAffinityMode(base::GetCurrentProcessHandle(), mode);

  base::UmaHistogramBoolean(
      "Power.CpuAffinityExperiments.ProcessAffinityUpdateSuccess", success);
}

class CpuAffinityTaskObserver : public base::TaskObserver {
 public:
  CpuAffinityTaskObserver() {}

  static CpuAffinityTaskObserver* GetInstance() {
    static base::NoDestructor<CpuAffinityTaskObserver> instance;
    return instance.get();
  }

  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override {}

  void DidProcessTask(const base::PendingTask& pending_task) override {
    ++task_counter_;
    if (task_counter_ == kUpdateAfterEveryNTasks) {
      if (enforced_mode_ &&
          *enforced_mode_ != base::CurrentThreadCpuAffinityMode()) {
        ApplyProcessCpuAffinityMode(*enforced_mode_);
      }
      task_counter_ = 0;
    }
  }

  void InitializeCpuAffinity(base::CpuAffinityMode mode) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // For now, affinity modes only have an effect on big.LITTLE architectures.
    if (!base::HasBigCpuCores())
      return;

    enforced_mode_ = mode;
    ApplyProcessCpuAffinityMode(mode);

    // Set up task observer if it's already possible. Otherwise it will be set
    // up after thread initialization (see app/content_main_runner_impl.cc and
    // child/child_process.cc).
    if (base::CurrentThread::IsSet())
      SetupCpuAffinityPolling();
  }

  void SetupCpuAffinityPollingOnce() {
    // The polling should be set up once from the main thread. Subsequent calls
    // from other threads should be ignored.
    if (did_call_setup_)
      return;

    SetupCpuAffinityPolling();
    did_call_setup_ = true;
  }

 private:
  void SetupCpuAffinityPolling() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!base::HasBigCpuCores() || !enforced_mode_ || did_setup_polling_)
      return;

    base::CurrentThread::Get()->AddTaskObserver(this);
    did_setup_polling_ = true;
  }

  SEQUENCE_CHECKER(sequence_checker_);

  static constexpr int kUpdateAfterEveryNTasks = 100;
  int task_counter_ = 0;
  bool did_setup_polling_ = false;
  bool did_call_setup_ = false;
  base::Optional<base::CpuAffinityMode> enforced_mode_;
};

}  // anonymous namespace

namespace content {

void EnforceProcessCpuAffinity(base::CpuAffinityMode mode) {
  CpuAffinityTaskObserver::GetInstance()->InitializeCpuAffinity(mode);
}

void SetupCpuAffinityPollingOnce() {
  CpuAffinityTaskObserver::GetInstance()->SetupCpuAffinityPollingOnce();
}

}  // namespace content

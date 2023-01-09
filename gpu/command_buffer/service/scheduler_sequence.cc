// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/scheduler_sequence.h"

#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_local.h"
#include "gpu/command_buffer/service/scheduler.h"

namespace gpu {

namespace {

#if DCHECK_IS_ON()
base::ThreadLocalBoolean* GetScheduleTaskDisallowed() {
  static base::NoDestructor<base::ThreadLocalBoolean> disallowed;
  return disallowed.get();
}
#endif  // DCHECK_IS_ON()

}  // namespace

ScopedAllowScheduleGpuTask::ScopedAllowScheduleGpuTask()
#if DCHECK_IS_ON()
    : original_value_(GetScheduleTaskDisallowed()->Get())
#endif  // DCHECK_IS_ON()
{
#if DCHECK_IS_ON()
  GetScheduleTaskDisallowed()->Set(false);
#endif  // DCHECK_IS_ON()
}

ScopedAllowScheduleGpuTask::~ScopedAllowScheduleGpuTask() {
#if DCHECK_IS_ON()
  GetScheduleTaskDisallowed()->Set(original_value_);
#endif  // DCHECK_IS_ON()
}

// static
void SchedulerSequence::DefaultDisallowScheduleTaskOnCurrentThread() {
#if DCHECK_IS_ON()
  GetScheduleTaskDisallowed()->Set(true);
#endif
}

SchedulerSequence::SchedulerSequence(
    Scheduler* scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool target_thread_is_always_available)
    : SingleTaskSequence(),
      scheduler_(scheduler),
      sequence_id_(scheduler->CreateSequence(SchedulingPriority::kHigh,
                                             std::move(task_runner))),
      target_thread_is_always_available_(target_thread_is_always_available) {}

// Note: this drops tasks not executed yet.
SchedulerSequence::~SchedulerSequence() {
  scheduler_->DestroySequence(sequence_id_);
}

// SingleTaskSequence implementation.
SequenceId SchedulerSequence::GetSequenceId() {
  return sequence_id_;
}

bool SchedulerSequence::ShouldYield() {
  return scheduler_->ShouldYield(sequence_id_);
}

void SchedulerSequence::ScheduleTask(base::OnceClosure task,
                                     std::vector<SyncToken> sync_token_fences,
                                     ReportingCallback report_callback) {
  // If your CL is failing this DCHECK, then that means you are probably calling
  // ScheduleGpuTask at a point that cannot be supported by Android Webview.
  // Consider using ScheduleOrRetainGpuTask which will delay (not reorder) the
  // task in Android Webview until the next DrawAndSwap.
  if (!target_thread_is_always_available_) {
#if DCHECK_IS_ON()
    DCHECK(!GetScheduleTaskDisallowed()->Get())
        << "If your CL is failing this DCHECK, then that means you are "
           "probably calling ScheduleGpuTask at a point that cannot be "
           "supported by Android Webview. Consider using "
           "ScheduleOrRetainGpuTask, which will delay (not reorder) the task "
           "in Android Webview until the next DrawAndSwap.";
#endif
  }

  ScheduleOrRetainTask(std::move(task), std::move(sync_token_fences),
                       std::move(report_callback));
}

void SchedulerSequence::ScheduleOrRetainTask(
    base::OnceClosure task,
    std::vector<gpu::SyncToken> sync_token_fences,
    ReportingCallback report_callback) {
  scheduler_->ScheduleTask(Scheduler::Task(sequence_id_, std::move(task),
                                           std::move(sync_token_fences),
                                           std::move(report_callback)));
}

void SchedulerSequence::ContinueTask(base::OnceClosure task) {
  scheduler_->ContinueTask(sequence_id_, std::move(task));
}

}  // namespace gpu

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/gpu_task_scheduler.h"

#include "base/functional/callback_helpers.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/scheduler_task_runner.h"

namespace webnn {

GpuTaskScheduler::GpuTaskScheduler(gpu::Scheduler& scheduler,
                                   gpu::CommandBufferId command_buffer_id,
                                   gpu::SequenceId sequence_id,
                                   gpu::CommandBufferNamespace namespace_id)
    : scheduler_(scheduler),
      command_buffer_id_(command_buffer_id),
      namespace_id_(namespace_id),
      sequence_id_(sequence_id),
      scheduler_task_runner_(
          base::MakeRefCounted<gpu::SchedulerTaskRunner>(*scheduler_,
                                                         sequence_id_)) {
  // Detach so the checker binds to whichever thread first schedules a task.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GpuTaskScheduler::~GpuTaskScheduler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: ShutDown() prevents new tasks from being scheduled and drops existing
  // ones from executing.
  scheduler_task_runner_->ShutDown();

  // Prevent pending scheduled tasks from running after DestroySequence() is
  // called.
  weak_factory_.InvalidateWeakPtrs();
}

void GpuTaskScheduler::ScheduleGpuTask(base::OnceClosure task_closure,
                                       gpu::SyncToken fence,
                                       gpu::SyncToken release) {
  ScheduleGpuTaskImpl(std::move(task_closure), {fence}, release);
}

void GpuTaskScheduler::ScheduleGpuTaskImpl(base::OnceClosure task_closure,
                                           std::vector<gpu::SyncToken> fences,
                                           const gpu::SyncToken& release) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::OnceClosure runnable_task = base::BindOnce(
      [](base::WeakPtr<GpuTaskScheduler> self, base::OnceClosure task) {
        // Tasks scheduled are wrapped in a WeakPtr check. If the sequence is
        // destroyed, the task is skipped, but the SyncToken is still signaled
        // by the scheduler to prevent deadlocks.
        if (self) {
          std::move(task).Run();
        }
      },
      weak_factory_.GetWeakPtr(), std::move(task_closure));

  scheduler_->ScheduleTask(gpu::Scheduler::Task(
      sequence_id_, std::move(runnable_task), std::move(fences), release));
}

}  // namespace webnn

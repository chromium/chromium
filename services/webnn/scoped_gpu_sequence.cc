// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/scoped_gpu_sequence.h"

#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/scheduler_task_runner.h"

namespace webnn {

ScopedGpuSequence::ScopedGpuSequence(
    gpu::Scheduler& scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gpu::CommandBufferId command_buffer_id,
    gpu::CommandBufferNamespace namespace_id)
    : scheduler_(scheduler),
      command_buffer_id_(command_buffer_id),
      namespace_id_(namespace_id),
      sequence_id_(scheduler_->CreateSequence(gpu::SchedulingPriority::kNormal,
                                              std::move(task_runner),
                                              namespace_id_,
                                              command_buffer_id_)),
      scheduler_task_runner_(
          base::MakeRefCounted<gpu::SchedulerTaskRunner>(*scheduler_,
                                                         sequence_id_)) {
  // Detach so the checker binds to whichever thread first schedules a task.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ScopedGpuSequence::~ScopedGpuSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: ShutDown() prevents new tasks from being scheduled and drops existing
  // ones from executing.
  scheduler_task_runner_->ShutDown();

  // Prevent pending scheduled tasks from running after DestroySequence() is
  // called.
  weak_factory_.InvalidateWeakPtrs();

  scheduler_->DestroySequence(sequence_id_);
}

void ScopedGpuSequence::ScheduleGpuTask(base::OnceClosure task_closure,
                                        gpu::SyncToken fence,
                                        gpu::SyncToken release) {
  ScheduleGpuTaskImpl(std::move(task_closure), {fence}, release);
}

void ScopedGpuSequence::ScheduleGpuTaskImpl(base::OnceClosure task_closure,
                                            std::vector<gpu::SyncToken> fences,
                                            const gpu::SyncToken& release) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::OnceClosure runnable_task = base::BindOnce(
      [](base::WeakPtr<ScopedGpuSequence> self, base::OnceClosure task) {
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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/gpu_task_scheduler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "gpu/command_buffer/service/scheduler.h"

namespace webnn {

GpuTaskScheduler::GpuTaskScheduler(
    gpu::Scheduler& scheduler,
    gpu::CommandBufferId command_buffer_id,
    gpu::CommandBufferNamespace namespace_id,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner)
    : scheduler_(scheduler),
      command_buffer_id_(command_buffer_id),
      namespace_id_(namespace_id),
      owning_task_runner_(std::move(owning_task_runner)) {
  // Detach so the checker binds to `owning_task_runner_` when initialized.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GpuTaskScheduler::~GpuTaskScheduler() {
  DestroySequence();
}

const scoped_refptr<base::SequencedTaskRunner>&
GpuTaskScheduler::GetTaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sequence_id_.is_null()) {
    DCHECK(owning_task_runner_->BelongsToCurrentThread());
    sequence_id_ = scheduler_->CreateSequence(
        gpu::SchedulingPriority::kNormal, owning_task_runner_, namespace_id_,
        command_buffer_id_);
    scheduler_task_runner_ = scheduler_->GetTaskRunner(sequence_id_);
  }
  return scheduler_task_runner_;
}

void GpuTaskScheduler::DestroySequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sequence_id_.is_null()) {
    return;
  }
  // Note: DestroySequence() shuts down `scheduler_task_runner_`, preventing new
  // tasks from being scheduled and dropping queued tasks.
  scheduler_->DestroySequence(sequence_id_);
  sequence_id_ = gpu::SequenceId();
  scheduler_task_runner_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

void GpuTaskScheduler::ScheduleGpuTask(base::OnceClosure task_closure,
                                       std::vector<gpu::SyncToken> fences,
                                       gpu::SyncToken release) {
  ScheduleGpuTaskImpl(std::move(task_closure), std::move(fences), release);
}

void GpuTaskScheduler::ScheduleGpuTaskImpl(base::OnceClosure task_closure,
                                           std::vector<gpu::SyncToken> fences,
                                           const gpu::SyncToken& release) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!sequence_id_.is_null());

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

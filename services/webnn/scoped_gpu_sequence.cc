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
                                                         sequence_id_)) {}

ScopedGpuSequence::~ScopedGpuSequence() {
  // Note: ShutDown() prevents new tasks from being scheduled and drops existing
  // ones from executing.
  scheduler_task_runner_->ShutDown();

  scheduler_->DestroySequence(sequence_id_);
}

void ScopedGpuSequence::WaitSyncToken(const gpu::SyncToken& fence) {
  // Prevent execution of scheduled tasks until the specified SyncToken fence
  // has been released.
  base::OnceClosure nop_task = base::DoNothing();
  scheduler_->ScheduleTask(
      gpu::Scheduler::Task(sequence_id_, std::move(nop_task), {fence}));
}

gpu::SyncToken ScopedGpuSequence::GenVerifiedSyncToken() {
  gpu::SyncToken verified_release(namespace_id_, command_buffer_id_,
                                  ++last_sync_token_release_id_);

  // Release the sync token once the sequence has completed execution by
  // appending a no-op task - the sync token will be automatically signaled
  // by the scheduler after this task executes.
  base::OnceClosure nop_task = base::DoNothing();
  scheduler_->ScheduleTask(gpu::Scheduler::Task(
      sequence_id_, std::move(nop_task), {}, verified_release));

  // Verify the release since the sync token could be passed to another Mojo
  // interface which requires verification. The release token was verified by
  // returning it to the renderer only after ScheduleTask was called.
  verified_release.SetVerifyFlush();
  return verified_release;
}

}  // namespace webnn

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/blocking_sequence_runner.h"

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/scheduler.h"

namespace gpu {

BlockingSequenceRunner::BlockingSequenceRunner(Scheduler* scheduler)
    : task_graph_(scheduler->task_graph()) {
  auto sequence = std::make_unique<Sequence>(scheduler);
  sequence_ = sequence.get();

  task_graph_->AddSequence(std::move(sequence));
}

BlockingSequenceRunner::~BlockingSequenceRunner() {
  SequenceId id = GetSequenceId();
  sequence_ = nullptr;
  task_graph_->DestroySequence(id);
}

SequenceId BlockingSequenceRunner::GetSequenceId() const {
  return sequence_->sequence_id();
}

bool BlockingSequenceRunner::HasTasks() const {
  base::AutoLock locker(lock());
  return sequence_->HasTasks();
}

uint32_t BlockingSequenceRunner::AddTask(TaskCallback task_callback,
                                         std::vector<SyncToken> wait_fences,
                                         const SyncToken& release,
                                         ReportingCallback report_callback) {
  base::AutoLock auto_lock(lock());
  return sequence_->AddTask(std::move(task_callback), std::move(wait_fences),
                            release, std::move(report_callback));
}

uint32_t BlockingSequenceRunner::AddTask(base::OnceClosure task_closure,
                                         std::vector<SyncToken> wait_fences,
                                         const SyncToken& release,
                                         ReportingCallback report_callback) {
  base::AutoLock auto_lock(lock());
  return sequence_->AddTask(std::move(task_closure), std::move(wait_fences),
                            release, std::move(report_callback));
}

ScopedSyncPointClientState BlockingSequenceRunner::CreateSyncPointClientState(
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id) {
  base::AutoLock auto_lock(lock());
  return sequence_->CreateSyncPointClientState(namespace_id, command_buffer_id);
}

void BlockingSequenceRunner::RunAllTasks() {
  base::AutoLock auto_lock(lock());
  sequence_->RunAllTasks();
}

BlockingSequenceRunner::Sequence::Sequence(Scheduler* scheduler)
    : TaskGraph::Sequence(scheduler->task_graph(),
                          /*validation_runner=*/{}),
      scheduler_(scheduler) {}

void BlockingSequenceRunner::Sequence::RunAllTasks() {
  while (!tasks_.empty()) {
    // Synchronously wait for the fences of the front task.
    while (!IsFrontTaskUnblocked()) {
      gpu::SyncToken sync_token = wait_fences_.begin()->sync_token;
      uint32_t order_num = wait_fences_.begin()->order_num;
      gpu::SequenceId release_sequence_id =
          wait_fences_.begin()->release_sequence_id;

      // Must unlock the task graph lock, otherwise it will deadlock when
      // calling into scheduler to update sequence priority, or when blocking on
      // `completion` waiting for other tasks to release fences.
      //
      // Manually release and re-acquire the lock, because locking annotation
      // used on ValidateSequenceTaskFenceDeps() doesn't recognize
      // base::AutoUnlock.
      lock().Release();

      base::WaitableEvent completion;
      if (task_graph_->sync_point_manager()->Wait(
              sync_token, sequence_id_, order_num,
              base::BindOnce(&base::WaitableEvent::Signal,
                             base::Unretained(&completion)))) {
        TRACE_EVENT1(
            "gpu",
            "BlockingSequenceRunner::Sequence::RunAllTasks::WaitSyncToken",
            "sequence_id", release_sequence_id.value());
        gpu::Scheduler::ScopedSetSequencePriority waiting(
            scheduler_, release_sequence_id, gpu::SchedulingPriority::kHigh);

        if (task_graph_->graph_validation_enabled()) {
          while (!completion.TimedWait(gpu::TaskGraph::kMinValidationDelay)) {
            task_graph_->ValidateSequenceTaskFenceDeps(this);
          }
        } else {
          completion.Wait();
        }
      }

      lock().Acquire();
    }

    // Run the front task.
    base::OnceClosure task_closure;
    uint32_t order_num = BeginTask(&task_closure);
    gpu::SyncToken release = current_task_release_;

    {
      base::AutoUnlock auto_unlock(lock());
      order_data()->BeginProcessingOrderNumber(order_num);

      std::move(task_closure).Run();

      if (order_data()->IsProcessingOrderNumber()) {
        if (release.HasData()) {
          task_graph_->sync_point_manager()->EnsureFenceSyncReleased(
              release, gpu::ReleaseCause::kTaskCompletionRelease);
        }

        order_data()->FinishProcessingOrderNumber(order_num);
      }
    }

    FinishTask();
  }
}

}  // namespace gpu

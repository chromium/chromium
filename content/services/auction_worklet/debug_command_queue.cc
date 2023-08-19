// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/debug_command_queue.h"

#include "base/containers/contains.h"
#include "base/task/sequenced_task_runner.h"

namespace auction_worklet {

DebugCommandQueue::DebugCommandQueue(
    scoped_refptr<base::SequencedTaskRunner> v8_runner)
    : v8_runner_(std::move(v8_runner)), wake_up_(&lock_) {}

DebugCommandQueue::~DebugCommandQueue() = default;

void DebugCommandQueue::PauseForDebuggerAndRunCommands(
    int context_group_id,
    base::OnceClosure abort_helper) {
  DCHECK(v8_runner_->RunsTasksInCurrentSequence());
  DCHECK(abort_helper);

  base::AutoLock auto_lock(lock_);
  CHECK(!v8_thread_paused_);
  DCHECK(!pause_abort_helper_);
  if (base::Contains(aborted_context_group_ids_, context_group_id)) {
    // Pauses disallowed since worklet is in process of being destroyed
    return;
  }

  v8_thread_paused_ = true;
  paused_context_group_id_ = context_group_id;
  pause_abort_helper_ = std::move(abort_helper);
  while (true) {
    RunQueueWithLockHeld();
    if (v8_thread_paused_)
      wake_up_.Wait();
    else
      break;
  }
  pause_abort_helper_.Reset();
}

void DebugCommandQueue::AbortPauses(int context_group_id) {
  base::AutoLock auto_lock(lock_);
  aborted_context_group_ids_.insert(context_group_id);

  if (v8_thread_paused_ && paused_context_group_id_ == context_group_id) {
    DCHECK(pause_abort_helper_);
    queue_.push(std::move(pause_abort_helper_));
    wake_up_.Signal();
  }
}

void DebugCommandQueue::RecycleContextGroupId(int context_group_id) {
  base::AutoLock auto_lock(lock_);
  size_t num_erased = aborted_context_group_ids_.erase(context_group_id);
  DCHECK_EQ(num_erased, 1u)
      << "DebugId::AbortDebuggerPauses must be called before ~DebugId.";
}

void DebugCommandQueue::QuitPauseForDebugger() {
  // Can be called from any thread.
  base::AutoLock auto_lock(lock_);
  v8_thread_paused_ = false;
  wake_up_.Signal();
}

void DebugCommandQueue::QueueTaskForV8Thread(base::OnceClosure task) {
  DCHECK(task);
  // Can be called from any thread.
  base::AutoLock auto_lock(lock_);
  queue_.push(std::move(task));
  if (v8_thread_paused_) {
    wake_up_.Signal();
  } else {
    PostRunQueue();
  }
}

void DebugCommandQueue::PostRunQueue() EXCLUSIVE_LOCKS_REQUIRED(lock_) {
  if (!queue_.empty()) {
    v8_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DebugCommandQueue::RunQueue, this));
  }
}

void DebugCommandQueue::RunQueue() {
  DCHECK(v8_runner_->RunsTasksInCurrentSequence());
  // Note: one of commands in the queue can cause PauseForDebuggerAndRunCommands
  // to be entered. This is OK since we pull tasks off one-by-one and run them
  // w/o a lock held.
  base::AutoLock auto_lock(lock_);
  RunQueueWithLockHeld();
}

void DebugCommandQueue::RunQueueWithLockHeld() EXCLUSIVE_LOCKS_REQUIRED(lock_) {
  DCHECK(v8_runner_->RunsTasksInCurrentSequence());
  bool was_v8_thread_paused_ = v8_thread_paused_;
  while (!queue_.empty()) {
    base::OnceClosure to_run = std::move(queue_.front());
    queue_.pop();
    {
      // Relinquish lock for running callback.
      base::AutoUnlock temporary_unlock(lock_);
      std::move(to_run).Run();
    }
    // Need to re-asses state here since it may have changed while lock was
    // released.
    if (was_v8_thread_paused_ && !v8_thread_paused_) {
      // QuitPauseForDebugger() was called, do the rest at top-level.
      PostRunQueue();
      return;
    }
  }
}

}  // namespace auction_worklet

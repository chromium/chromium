// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/debug_command_queue.h"

#include "base/threading/sequenced_task_runner_handle.h"

namespace auction_worklet {

DebugCommandQueue::DebugCommandQueue()
    : v8_runner_(base::SequencedTaskRunnerHandle::Get()), wake_up_(&lock_) {}

void DebugCommandQueue::PauseForDebuggerAndRunCommands() {
  DCHECK(v8_runner_->RunsTasksInCurrentSequence());

  base::AutoLock auto_lock(lock_);
  DCHECK(!v8_thread_paused_);
  v8_thread_paused_ = true;
  while (true) {
    RunQueueWithLockHeld();
    if (v8_thread_paused_)
      wake_up_.Wait();
    else
      break;
  }
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

DebugCommandQueue::~DebugCommandQueue() = default;

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

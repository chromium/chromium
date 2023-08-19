// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/devtools/debug_command_queue.h"

#include "base/task/sequenced_task_runner.h"

namespace ax {

DebugCommandQueue::DebugCommandQueue()
    : v8_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      wake_up_(&lock_) {}

DebugCommandQueue::~DebugCommandQueue() = default;

void DebugCommandQueue::QuitPauseForDebugger() {
  // Can be called from any thread.
  base::AutoLock auto_lock(lock_);
  wake_up_.Signal();
}

void DebugCommandQueue::QueueTaskForV8Thread(base::OnceClosure task) {
  DCHECK(task);
  // Can be called from any thread.
  base::AutoLock auto_lock(lock_);
  queue_.push(std::move(task));
  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&DebugCommandQueue::RunQueue, this));
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
  while (!queue_.empty()) {
    base::OnceClosure to_run = std::move(queue_.front());
    queue_.pop();
    {
      // Relinquish lock for running callback.
      base::AutoUnlock temporary_unlock(lock_);
      std::move(to_run).Run();
    }
  }
}

}  // namespace ax

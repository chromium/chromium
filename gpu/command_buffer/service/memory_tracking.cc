// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/memory_tracking.h"
#include "base/task/sequenced_task_runner.h"

namespace gpu {

// This can be created on the render thread on Andorid Webview which is managed
// by the OS and doesn't have a task runner associated with it in which case
// base::SequencedTaskRunner::GetCurrentDefault() will trigger a DCHECK.
MemoryTypeTracker::MemoryTypeTracker(MemoryTracker* memory_tracker)
    : MemoryTypeTracker(memory_tracker,
                        base::SequencedTaskRunner::HasCurrentDefault()
                            ? base::SequencedTaskRunner::GetCurrentDefault()
                            : nullptr) {}

MemoryTypeTracker::MemoryTypeTracker(
    MemoryTracker* memory_tracker,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : memory_tracker_(memory_tracker),
      task_runner_(std::move(task_runner)),
      weak_ptr_factory_(this) {}

MemoryTypeTracker::~MemoryTypeTracker() {
  DCHECK_EQ(mem_represented_, 0u);
}

void MemoryTypeTracker::TrackMemAlloc(size_t bytes) {
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(bytes >= 0);
    mem_represented_ += bytes;
  }
  // Notify memory tracker outside the lock to prevent reentrancy deadlock.
  if (memory_tracker_ && bytes)
    TrackMemoryAllocatedChange(bytes);
}

void MemoryTypeTracker::TrackMemFree(size_t bytes) {
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(bytes >= 0 && bytes <= mem_represented_);
    mem_represented_ -= bytes;
  }
  // Notify memory tracker outside the lock to prevent reentrancy deadlock.
  if (memory_tracker_ && bytes)
    TrackMemoryAllocatedChange(-static_cast<int64_t>(bytes));
}

size_t MemoryTypeTracker::GetMemRepresented() const {
  base::AutoLock auto_lock(lock_);
  return mem_represented_;
}

void MemoryTypeTracker::TrackMemoryAllocatedChange(int64_t delta) {
  DCHECK(memory_tracker_);
  if (task_runner_ && !task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MemoryTypeTracker::TrackMemoryAllocatedChange,
                       weak_ptr_factory_.GetWeakPtr(), delta));
  } else {
    memory_tracker_->TrackMemoryAllocatedChange(delta);
  }
}

}  // namespace gpu

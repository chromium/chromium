// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/buffer_task.h"

#include "services/webnn/tflite/buffer_state.h"

namespace webnn::tflite {

BufferTask::BufferTask(
    std::vector<scoped_refptr<BufferState>> shared_buffers,
    std::vector<scoped_refptr<BufferState>> exclusive_buffers,
    base::OnceCallback<void(base::OnceClosure)> task)
    : shared_buffers_(std::move(shared_buffers)),
      exclusive_buffers_(std::move(exclusive_buffers)),
      task_(std::move(task)) {}

void BufferTask::Enqueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (CanExecute()) {
    Execute(/*dequeue=*/false);
    return;
  }

  for (const auto& buffer : shared_buffers_) {
    buffer->EnqueueTask(this);
  }
  for (const auto& buffer : exclusive_buffers_) {
    buffer->EnqueueTask(this);
  }
}

BufferTask::~BufferTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(task_.is_null());
}

bool BufferTask::CanExecute() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& buffer : shared_buffers_) {
    if (!buffer->CanLock(/*exclusive=*/false)) {
      return false;
    }
    BufferTask* task = buffer->PeekTask();
    if (task && task != this) {
      return false;
    }
  }
  for (const auto& buffer : exclusive_buffers_) {
    if (!buffer->CanLock(/*exclusive=*/true)) {
      return false;
    }
    BufferTask* task = buffer->PeekTask();
    if (task && task != this) {
      return false;
    }
  }

  return true;
}

void BufferTask::Execute(bool dequeue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Popping this task from the buffer queues might release the last reference
  // to this object. Make sure to save one of these references on the stack.
  scoped_refptr<BufferTask> self;

  for (const auto& buffer : shared_buffers_) {
    if (dequeue) {
      self = buffer->PopTask();
      CHECK_EQ(this, self.get());
    }
    buffer->Lock(/*exclusive=*/false);
  }
  for (const auto& buffer : exclusive_buffers_) {
    if (dequeue) {
      self = buffer->PopTask();
      CHECK_EQ(this, self.get());
    }
    buffer->Lock(/*exclusive=*/true);
  }

  // `task_` may invoke the completion callback synchronously.
  std::move(task_).Run(base::BindOnce(&BufferTask::Complete, this));
}

void BufferTask::Complete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& buffer : shared_buffers_) {
    buffer->Unlock();
  }
  for (const auto& buffer : exclusive_buffers_) {
    buffer->Unlock();
  }

  for (const auto& buffer : shared_buffers_) {
    // A task that is waiting for a buffer with a shared lock must want an
    // exclusive lock and only one such task can run at once so we can stop
    // after finding the first task.
    BufferTask* task = buffer->PeekTask();
    if (task && task->CanExecute()) {
      task->Execute(/*dequeue=*/true);
    }
  }
  for (const auto& buffer : exclusive_buffers_) {
    // Multiple tasks requiring a shared lock could be waiting for this buffer
    // to be unlocked, so try to run as many executable tasks as possible.
    while (BufferTask* task = buffer->PeekTask()) {
      if (task->CanExecute()) {
        task->Execute(/*dequeue=*/true);
      } else {
        break;
      }
    }
  }
}

}  // namespace webnn::tflite

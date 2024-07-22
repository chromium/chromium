// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/buffer_state.h"

#include "services/webnn/tflite/buffer_content.h"
#include "services/webnn/tflite/buffer_task.h"

namespace webnn::tflite {

BufferState::BufferState(size_t size)
    : content_(base::MakeRefCounted<BufferContent>(size)) {}

bool BufferState::CanLock(bool exclusive) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_ == State::kUnlocked ||
         (state_ == State::kLockedShared && !exclusive);
}

void BufferState::Lock(bool exclusive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(CanLock(exclusive));
  state_ = exclusive ? State::kLockedExclusive : State::kLockedShared;
}

void BufferState::Unlock() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kUnlocked;
}

void BufferState::EnqueueTask(scoped_refptr<BufferTask> task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  waiting_tasks_.push(std::move(task));
}

BufferTask* BufferState::PeekTask() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return waiting_tasks_.empty() ? nullptr : waiting_tasks_.front().get();
}

scoped_refptr<BufferTask> BufferState::PopTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!waiting_tasks_.empty());
  scoped_refptr<BufferTask> task = std::move(waiting_tasks_.front());
  waiting_tasks_.pop();
  return task;
}

const scoped_refptr<BufferContent>& BufferState::GetContent() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content_;
}

BufferState::~BufferState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace webnn::tflite

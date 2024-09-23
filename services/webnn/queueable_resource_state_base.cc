// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/queueable_resource_state_base.h"

#include "services/webnn/resource_task.h"

namespace webnn {

bool QueueableResourceStateBase::CanLock(bool exclusive) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_ == State::kUnlocked ||
         (state_ == State::kLockedShared && !exclusive);
}

void QueueableResourceStateBase::Lock(bool exclusive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(CanLock(exclusive));
  state_ = exclusive ? State::kLockedExclusive : State::kLockedShared;
}

void QueueableResourceStateBase::Unlock() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kUnlocked;
}

void QueueableResourceStateBase::EnqueueTask(scoped_refptr<ResourceTask> task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  waiting_tasks_.push(std::move(task));
}
ResourceTask* QueueableResourceStateBase::PeekTask() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return waiting_tasks_.empty() ? nullptr : waiting_tasks_.front().get();
}
scoped_refptr<ResourceTask> QueueableResourceStateBase::PopTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!waiting_tasks_.empty());
  scoped_refptr<ResourceTask> task = std::move(waiting_tasks_.front());
  waiting_tasks_.pop();
  return task;
}

bool QueueableResourceStateBase::IsLockedShared() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_ == State::kLockedShared;
}

bool QueueableResourceStateBase::IsLockedExclusive() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_ == State::kLockedExclusive;
}

QueueableResourceStateBase::QueueableResourceStateBase() = default;

QueueableResourceStateBase::~QueueableResourceStateBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace webnn

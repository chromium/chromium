// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/serial_worker.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"

namespace net {

SerialWorker::SerialWorker()
    : base::RefCountedDeleteOnSequence<SerialWorker>(
          base::SequencedTaskRunnerHandle::Get()),
      state_(State::kIdle) {}

SerialWorker::~SerialWorker() = default;

void SerialWorker::WorkNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case State::kIdle:
      // We are posting weak pointer to OnWorkJobFinished to avoid leak when
      // PostTaskAndReply fails to post task back to the original
      // task runner. In this case the callback is not destroyed, and the
      // weak reference allows SerialWorker instance to be deleted.
      base::ThreadPool::PostTaskAndReply(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          base::BindOnce(&SerialWorker::DoWork, this),
          base::BindOnce(&SerialWorker::OnWorkJobFinished,
                         weak_factory_.GetWeakPtr()));
      state_ = State::kWorking;
      return;
    case State::kWorking:
      // Remember to re-read after |DoRead| finishes.
      state_ = State::kPending;
      return;
    case State::kCancelled:
    case State::kPending:
      return;
    default:
      NOTREACHED() << "Unexpected state " << static_cast<int>(state_);
  }
}

void SerialWorker::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kCancelled;
}

void SerialWorker::OnWorkJobFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case State::kCancelled:
      return;
    case State::kWorking:
      state_ = State::kIdle;
      this->OnWorkFinished();
      return;
    case State::kPending:
      state_ = State::kIdle;
      WorkNow();
      return;
    default:
      NOTREACHED() << "Unexpected state " << static_cast<int>(state_);
  }
}

}  // namespace net

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
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"

namespace net {

namespace {
// Default retry configuration. Only in effect if |max_number_of_retries| is
// greater than 0.
constexpr BackoffEntry::Policy kDefaultBackoffPolicy = {
    0,     // Number of initial errors to ignore without backoff.
    5000,  // Initial delay for backoff in ms: 5 seconds.
    2,     // Factor to multiply for exponential backoff.
    0,     // Fuzzing percentage.
    -1,    // No maximum delay.
    -1,    // Don't discard entry.
    false  // Don't use initial delay unless the last was an error.
};
}  // namespace

SerialWorker::SerialWorker(int max_number_of_retries,
                           const net::BackoffEntry::Policy* backoff_policy)
    : base::RefCountedDeleteOnSequence<SerialWorker>(
          base::SequencedTaskRunnerHandle::Get()),
      state_(IDLE),
      max_number_of_retries_(max_number_of_retries),
      backoff_entry_(backoff_policy != nullptr ? backoff_policy
                                               : &kDefaultBackoffPolicy) {}

SerialWorker::~SerialWorker() = default;

void SerialWorker::WorkNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Not a retry; reset failure count and cancel the pending retry (if any).
  backoff_entry_.Reset();
  retry_timer_.Stop();
  WorkNowInternal();
}

void SerialWorker::WorkNowInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case IDLE:
      // We are posting weak pointer to OnWorkJobFinished to avoid a leak when
      // PostTaskAndReply fails to post task back to the original
      // task runner. In this case the callback is not destroyed, and the
      // weak reference allows SerialWorker instance to be deleted.
      base::ThreadPool::PostTaskAndReply(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          base::BindOnce(&SerialWorker::DoWork, this),
          base::BindOnce(&SerialWorker::OnWorkJobFinished,
                         weak_factory_.GetWeakPtr()));
      state_ = WORKING;
      return;
    case WORKING:
      // Remember to re-read after |DoRead| finishes.
      state_ = PENDING;
      return;
    case CANCELLED:
    case PENDING:
      return;
    default:
      NOTREACHED() << "Unexpected state " << state_;
  }
}

void SerialWorker::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = CANCELLED;
}
void SerialWorker::OnWorkJobFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case CANCELLED:
      return;
    case WORKING:
      state_ = IDLE;
      if (this->OnWorkFinished() ||
          backoff_entry_.failure_count() >= max_number_of_retries_) {
        backoff_entry_.Reset();
      } else {
        backoff_entry_.InformOfRequest(/*succeeded=*/false);

        // Try again after a delay.
        retry_timer_.Start(FROM_HERE, backoff_entry_.GetTimeUntilRelease(),
                           this, &SerialWorker::WorkNowInternal);
      }
      return;
    case PENDING:
      state_ = IDLE;
      WorkNowInternal();
      return;
    default:
      NOTREACHED() << "Unexpected state " << state_;
  }
}

const BackoffEntry& SerialWorker::GetBackoffEntryForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return backoff_entry_;
}

const base::OneShotTimer& SerialWorker::GetRetryTimerForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return retry_timer_;
}

}  // namespace net

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/serial_worker.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
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

namespace {
std::unique_ptr<SerialWorker::WorkItem> DoWork(
    std::unique_ptr<SerialWorker::WorkItem> work_item) {
  DCHECK(work_item);
  work_item->DoWork();
  return work_item;
}
}  // namespace

void SerialWorker::WorkItem::FollowupWork(base::OnceClosure closure) {
  std::move(closure).Run();
}

SerialWorker::SerialWorker(int max_number_of_retries,
                           const net::BackoffEntry::Policy* backoff_policy)
    : max_number_of_retries_(max_number_of_retries),
      backoff_entry_(backoff_policy ? backoff_policy : &kDefaultBackoffPolicy) {
}

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
    case State::kIdle:
      // We are posting weak pointer to OnWorkJobFinished to avoid a leak when
      // PostTaskAndReply fails to post task back to the original
      // task runner. In this case the callback is not destroyed, and the
      // weak reference allows SerialWorker instance to be deleted.
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          base::BindOnce(&DoWork, CreateWorkItem()),
          base::BindOnce(&SerialWorker::OnDoWorkFinished, AsWeakPtr()));
      state_ = State::kWorking;
      return;
    case State::kWorking:
      // Remember to re-read after `DoWork()` finishes.
      state_ = State::kPending;
      return;
    case State::kCancelled:
    case State::kPending:
      return;
  }
}

void SerialWorker::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kCancelled;
}

void SerialWorker::OnDoWorkFinished(std::unique_ptr<WorkItem> work_item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (state_) {
    case State::kCancelled:
      return;
    case State::kWorking: {
      WorkItem* work_item_ptr = work_item.get();
      work_item_ptr->FollowupWork(
          base::BindOnce(&SerialWorker::OnFollowupWorkFinished,
                         weak_factory_.GetWeakPtr(), std::move(work_item)));
      return;
    }
    case State::kPending: {
      RerunWork(std::move(work_item));
      return;
    }
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected state " << static_cast<int>(state_);
  }
}

void SerialWorker::OnFollowupWorkFinished(std::unique_ptr<WorkItem> work_item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case State::kCancelled:
      return;
    case State::kWorking:
      state_ = State::kIdle;
      if (OnWorkFinished(std::move(work_item)) ||
          backoff_entry_.failure_count() >= max_number_of_retries_) {
        backoff_entry_.Reset();
      } else {
        backoff_entry_.InformOfRequest(/*succeeded=*/false);

        // Try again after a delay.
        retry_timer_.Start(FROM_HERE, backoff_entry_.GetTimeUntilRelease(),
                           this, &SerialWorker::WorkNowInternal);
      }
      return;
    case State::kPending:
      RerunWork(std::move(work_item));
      return;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected state " << static_cast<int>(state_);
  }
}

void SerialWorker::RerunWork(std::unique_ptr<WorkItem> work_item) {
  // `WorkNow()` was retriggered while working, so need to redo work
  // immediately to ensure up-to-date results. Reuse `work_item` rather than
  // returning it to the derived class (and letting it potentially act on a
  // potential obsolete result).
  DCHECK_EQ(state_, State::kPending);
  state_ = State::kWorking;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DoWork, std::move(work_item)),
      base::BindOnce(&SerialWorker::OnDoWorkFinished, AsWeakPtr()));
}

const BackoffEntry& SerialWorker::GetBackoffEntryForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return backoff_entry_;
}

const base::OneShotTimer& SerialWorker::GetRetryTimerForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return retry_timer_;
}

int SerialWorker::GetFailureCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return backoff_entry_.failure_count();
}

base::WeakPtr<SerialWorker> SerialWorker::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace net

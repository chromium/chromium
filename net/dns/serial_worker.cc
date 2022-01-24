// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/serial_worker.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"

namespace net {

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

SerialWorker::SerialWorker() : state_(State::kIdle) {}

SerialWorker::~SerialWorker() = default;

void SerialWorker::WorkNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case State::kIdle:
      // We are posting weak pointer to OnWorkJobFinished to avoid leak when
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
      NOTREACHED() << "Unexpected state " << static_cast<int>(state_);
  }
}

void SerialWorker::OnFollowupWorkFinished(std::unique_ptr<WorkItem> work_item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case State::kCancelled:
      return;
    case State::kWorking:
      state_ = State::kIdle;
      OnWorkFinished(std::move(work_item));
      return;
    case State::kPending:
      RerunWork(std::move(work_item));
      return;
    default:
      NOTREACHED() << "Unexpected state " << static_cast<int>(state_);
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

base::WeakPtr<SerialWorker> SerialWorker::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace net

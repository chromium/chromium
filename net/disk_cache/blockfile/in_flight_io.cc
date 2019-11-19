// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/in_flight_io.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"

namespace disk_cache {

BackgroundIO::BackgroundIO(InFlightIO* controller)
    : result_(-1),
      io_completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                    base::WaitableEvent::InitialState::NOT_SIGNALED),
      controller_(controller) {}

// Runs on the primary thread.
void BackgroundIO::OnIOSignalled() {
  if (controller_)
    controller_->InvokeCallback(this, false);
}

void BackgroundIO::Cancel() {
  // controller_ may be in use from the background thread at this time.
  base::AutoLock lock(controller_lock_);
  DCHECK(controller_);
  controller_ = nullptr;
}

BackgroundIO::~BackgroundIO() = default;

// ---------------------------------------------------------------------------

InFlightIO::InFlightIO()
    : callback_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      running_(false) {}

InFlightIO::~InFlightIO() = default;

// Runs on the background thread.
void BackgroundIO::NotifyController() {
  base::AutoLock lock(controller_lock_);
  if (controller_)
    controller_->OnIOComplete(this);
}

void InFlightIO::WaitForPendingIO() {
  while (!io_list_.empty()) {
    // Block the current thread until all pending IO completes.
    auto it = io_list_.begin();
    InvokeCallback(it->get(), true);
  }
}

void InFlightIO::DropPendingIO() {
  while (!io_list_.empty()) {
    auto it = io_list_.begin();
    BackgroundIO* operation = it->get();
    operation->Cancel();
    DCHECK(io_list_.find(operation) != io_list_.end());
    io_list_.erase(base::WrapRefCounted(operation));
  }
}

// Runs in a background sequence.
void InFlightIO::OnIOComplete(BackgroundIO* operation) {
#if DCHECK_IS_ON()
  if (callback_task_runner_->RunsTasksInCurrentSequence()) {
    DCHECK(single_thread_ || !running_);
    single_thread_ = true;
  }
#endif

  callback_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundIO::OnIOSignalled, operation));
  operation->io_completed()->Signal();
}

// Runs on the primary thread.
void InFlightIO::InvokeCallback(BackgroundIO* operation, bool cancel_task) {
  {
    // http://crbug.com/74623
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    operation->io_completed()->Wait();
  }
  running_ = true;

  if (cancel_task)
    operation->Cancel();

  // Make sure that we remove the operation from the list before invoking the
  // callback (so that a subsequent cancel does not invoke the callback again).
  DCHECK(io_list_.find(operation) != io_list_.end());
  DCHECK(!operation->HasOneRef());
  io_list_.erase(base::WrapRefCounted(operation));
  OnOperationComplete(operation, cancel_task);
}

// Runs on the primary thread.
void InFlightIO::OnOperationPosted(BackgroundIO* operation) {
  DCHECK(callback_task_runner_->RunsTasksInCurrentSequence());
  io_list_.insert(base::WrapRefCounted(operation));
}

}  // namespace disk_cache

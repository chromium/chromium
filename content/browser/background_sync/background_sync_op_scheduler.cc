// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_op_scheduler.h"

#include <string>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"

namespace content {

class BackgroundSyncOpScheduler::Operation {
 public:
  explicit Operation(base::OnceClosure closure)
      : closure_(std::move(closure)) {}
  ~Operation() = default;

  Operation(const Operation&) = delete;
  const Operation& operator=(const Operation&) = delete;

  // Run the closure passed to the constructor.
  void Run() { std::move(closure_).Run(); }

  base::WeakPtr<Operation> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::OnceClosure closure_;

  base::WeakPtrFactory<Operation> weak_ptr_factory_{this};
};

BackgroundSyncOpScheduler::BackgroundSyncOpScheduler(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

BackgroundSyncOpScheduler::~BackgroundSyncOpScheduler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BackgroundSyncOpScheduler::ScheduleOperation(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_operations_.emplace(
      std::make_unique<BackgroundSyncOpScheduler::Operation>(
          std::move(closure)));
  MaybeRunOperation();
}

void BackgroundSyncOpScheduler::CompleteOperationAndRunNext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(running_operation_);
  running_operation_ = nullptr;

  MaybeRunOperation();
}

bool BackgroundSyncOpScheduler::ScheduledOperations() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return running_operation_ || !pending_operations_.empty();
}

void BackgroundSyncOpScheduler::MaybeRunOperation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If there are no operations, then we can't run anything.
  if (pending_operations_.empty()) {
    DoneStartingAvailableOperations();
    return;
  }

  if (running_operation_) {
    DoneStartingAvailableOperations();
    return;
  }

  running_operation_ = std::move(pending_operations_.front());
  pending_operations_.pop();

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundSyncOpScheduler::Operation::Run,
                                running_operation_->AsWeakPtr()));

  DoneStartingAvailableOperations();
}

}  // namespace content

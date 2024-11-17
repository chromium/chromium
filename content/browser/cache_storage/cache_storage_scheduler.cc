// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_scheduler.h"

#include <string>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "content/browser/cache_storage/cache_storage_operation.h"

namespace content {

namespace {

// Maximum parallel shared operations.  This constant was selected via
// experimentation.  We tried 4, 16, and 64 for the limit.  16 was clearly
// better than 4, but 64 was did not provide significant further benefit.
constexpr int kDefaultMaxSharedOps = 16;

const base::FeatureParam<int> kCacheStorageMaxSharedOps{
    &kCacheStorageParallelOps, "max_shared_ops", kDefaultMaxSharedOps};

bool OpPointerLessThan(const std::unique_ptr<CacheStorageOperation>& left,
                       const std::unique_ptr<CacheStorageOperation>& right) {
  DCHECK(left);
  DCHECK(right);
  // We want to prioritize high priority operations, but otherwise sort
  // by creation order.  Since the first created operations will have a lower
  // identifier value we reverse the logic of the id comparison.
  //
  // Note, there might be a slight mis-ordering when the 64-bit id values
  // rollover, but this should not be critical and will happen very rarely.
  if (left->priority() < right->priority()) {
    return true;
  }
  if (left->priority() > right->priority()) {
    return false;
  }
  return left->id() > right->id();
}

}  // namespace

// Enables support for parallel cache_storage operations via the
// "max_shared_ops" fieldtrial parameter.
BASE_FEATURE(kCacheStorageParallelOps,
             "CacheStorageParallelOps",
             base::FEATURE_ENABLED_BY_DEFAULT);

CacheStorageScheduler::CacheStorageScheduler(
    CacheStorageSchedulerClient client_type,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)), client_type_(client_type) {
  std::make_heap(pending_operations_.begin(), pending_operations_.end(),
                 &OpPointerLessThan);
}

CacheStorageScheduler::~CacheStorageScheduler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CacheStorageSchedulerId CacheStorageScheduler::CreateId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return next_id_++;
}

void CacheStorageScheduler::ScheduleOperation(
    CacheStorageSchedulerId id,
    CacheStorageSchedulerMode mode,
    CacheStorageSchedulerOp op_type,
    CacheStorageSchedulerPriority priority,
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordCacheStorageSchedulerUMA(CacheStorageSchedulerUMA::kQueueLength,
                                 client_type_, op_type,
                                 pending_operations_.size());

  pending_operations_.push_back(std::make_unique<CacheStorageOperation>(
      std::move(closure), id, client_type_, mode, op_type, priority,
      task_runner_));
  std::push_heap(pending_operations_.begin(), pending_operations_.end(),
                 &OpPointerLessThan);
  MaybeRunOperation();
}

void CacheStorageScheduler::CompleteOperationAndRunNext(
    CacheStorageSchedulerId id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = running_operations_.find(id);
  CHECK(it != running_operations_.end(), base::NotFatalUntil::M130);
  DCHECK_EQ(it->second->id(), id);

  if (it->second->mode() == CacheStorageSchedulerMode::kShared) {
    DCHECK_EQ(num_running_exclusive_, 0);
    DCHECK_GT(num_running_shared_, 0);
    num_running_shared_ -= 1;
  } else {
    DCHECK_EQ(num_running_shared_, 0);
    DCHECK_EQ(num_running_exclusive_, 1);
    num_running_exclusive_ -= 1;
  }

  running_operations_.erase(it);

  MaybeRunOperation();
}

bool CacheStorageScheduler::ScheduledOperations() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !running_operations_.empty() || !pending_operations_.empty();
}

bool CacheStorageScheduler::IsRunningExclusiveOperation() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return num_running_exclusive_ > 0;
}

void CacheStorageScheduler::DispatchOperationTask(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(task).Run();
}

void CacheStorageScheduler::MaybeRunOperation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Most operations are wrapped with `CompleteOperationAndRunNext()`, which
  // means that executing an operation can cause re-entrancy in
  // `MaybeRunOperation().` No-op in this case; the next operation will be run
  // in the next loop iteration. Note that this doesn't use `AutoReset` because
  // running an operation can cause `this` to be deleted. See
  // https://crbug.com/370069678.
  if (in_maybe_run_) {
    return;
  }
  in_maybe_run_ = true;
  base::WeakPtr<CacheStorageScheduler> this_ptr =
      weak_ptr_factory_.GetWeakPtr();
  base::ScopedClosureRunner reset(base::BindOnce(
      [](base::WeakPtr<CacheStorageScheduler> scheduler) {
        if (scheduler) {
          scheduler->in_maybe_run_ = false;
        }
      },
      this_ptr));

  while (this_ptr && !pending_operations_.empty()) {
    base::WeakPtr<CacheStorageOperation> next_operation =
        pending_operations_.front()->AsWeakPtr();

    // Determine if we can run the next operation based on its mode
    // and the current state of executing operations.  We allow multiple
    // kShared operations to run in parallel, but a kExclusive operation
    // must not overlap with any other operation.
    if (num_running_exclusive_ > 0) {
      return;
    }
    const bool is_shared_op =
        next_operation->mode() == CacheStorageSchedulerMode::kShared;
    const int max_concurrent_ops =
        is_shared_op ? kCacheStorageMaxSharedOps.Get() : 1;
    if (num_running_shared_ >= max_concurrent_ops) {
      return;
    }

    running_operations_.emplace(next_operation->id(),
                                std::move(pending_operations_.front()));
    std::pop_heap(pending_operations_.begin(), pending_operations_.end(),
                  &OpPointerLessThan);
    pending_operations_.pop_back();

    RecordCacheStorageSchedulerUMA(
        CacheStorageSchedulerUMA::kQueueDuration, client_type_,
        next_operation->op_type(),
        base::TimeTicks::Now() - next_operation->creation_ticks());

    if (is_shared_op) {
      CHECK_EQ(num_running_exclusive_, 0);
      num_running_shared_ += 1;
    } else {
      CHECK_EQ(num_running_exclusive_, 0);
      CHECK_EQ(num_running_shared_, 0);
      num_running_exclusive_ += 1;
    }

    DispatchOperationTask(
        base::BindOnce(&CacheStorageOperation::Run, next_operation));
    // `next_operation` and `this_ptr` may both be null at this point.
  }
}

}  // namespace content

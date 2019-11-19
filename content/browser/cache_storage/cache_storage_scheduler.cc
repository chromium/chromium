// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_scheduler.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"
#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "content/browser/cache_storage/cache_storage_operation.h"
#include "content/public/common/content_features.h"

namespace content {

namespace {

// Maximum parallel shared operations.  This constant was selected via
// experimentation.  We tried 4, 16, and 64 for the limit.  16 was clearly
// better than 4, but 64 was did not provide significant further benefit.
// TODO(crbug/1007994): Enable parallel shared operations on android after
//                      performance regressions are addressed.
#if defined(OS_ANDROID)
constexpr int kDefaultMaxSharedOps = 1;
#else
constexpr int kDefaultMaxSharedOps = 16;
#endif

const base::FeatureParam<int> kCacheStorageMaxSharedOps{
    &features::kCacheStorageParallelOps, "max_shared_ops",
    kDefaultMaxSharedOps};

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
  DCHECK(it != running_operations_.end());
  DCHECK_EQ(it->second->id(), id);

  if (it->second->mode() == CacheStorageSchedulerMode::kShared) {
    DCHECK_EQ(num_running_exclusive_, 0);
    DCHECK_GT(num_running_shared_, 0);
    num_running_shared_ -= 1;
    if (num_running_shared_ == 0) {
      UMA_HISTOGRAM_COUNTS_100("ServiceWorkerCache.PeakParallelSharedOps2",
                               peak_parallel_shared_);
      peak_parallel_shared_ = 0;
    }
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
  task_runner_->PostTask(FROM_HERE, std::move(task));
}

void CacheStorageScheduler::MaybeRunOperation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If there are no operations, then we can't run anything.
  if (pending_operations_.empty()) {
    DoneStartingAvailableOperations();
    return;
  }

  auto* next_operation = pending_operations_.front().get();

  // Determine if we can run the next operation based on its mode
  // and the current state of executing operations.  We allow multiple
  // kShared operations to run in parallel, but a kExclusive operation
  // must not overlap with any other operation.
  if (next_operation->mode() == CacheStorageSchedulerMode::kShared) {
    if (num_running_exclusive_ > 0 ||
        num_running_shared_ >= kCacheStorageMaxSharedOps.Get()) {
      DoneStartingAvailableOperations();
      return;
    }
  } else if (num_running_shared_ > 0 || num_running_exclusive_ > 0) {
    DCHECK_EQ(next_operation->mode(), CacheStorageSchedulerMode::kExclusive);
    DoneStartingAvailableOperations();
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

  if (next_operation->mode() == CacheStorageSchedulerMode::kShared) {
    DCHECK_EQ(num_running_exclusive_, 0);
    num_running_shared_ += 1;
    peak_parallel_shared_ =
        std::max(num_running_shared_, peak_parallel_shared_);
  } else {
    DCHECK_EQ(num_running_exclusive_, 0);
    DCHECK_EQ(num_running_shared_, 0);
    num_running_exclusive_ += 1;
  }

  DispatchOperationTask(
      base::BindOnce(&CacheStorageOperation::Run, next_operation->AsWeakPtr()));

  // If we just executed a kShared operation, then we may be able to schedule
  // additional kShared parallel operations.  Recurse to process the next
  // pending operation.
  if (next_operation->mode() == CacheStorageSchedulerMode::kShared)
    MaybeRunOperation();
  else
    DoneStartingAvailableOperations();
}

}  // namespace content

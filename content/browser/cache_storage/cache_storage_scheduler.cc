// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_scheduler.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "content/browser/cache_storage/cache_storage_operation.h"

namespace content {

CacheStorageScheduler::CacheStorageScheduler(
    CacheStorageSchedulerClient client_type)
    : client_type_(client_type), weak_ptr_factory_(this) {}

CacheStorageScheduler::~CacheStorageScheduler() {}

void CacheStorageScheduler::ScheduleOperation(base::OnceClosure closure) {
  CACHE_STORAGE_SCHEDULER_UMA(COUNTS_10000, "QueueLength", client_type_,
                              pending_operations_.size());

  pending_operations_.push_back(std::make_unique<CacheStorageOperation>(
      std::move(closure), client_type_, base::ThreadTaskRunnerHandle::Get()));
  RunOperationIfIdle();
}

void CacheStorageScheduler::CompleteOperationAndRunNext() {
  DCHECK(running_operation_);
  running_operation_.reset();

  RunOperationIfIdle();
}

bool CacheStorageScheduler::ScheduledOperations() const {
  return running_operation_ || !pending_operations_.empty();
}

void CacheStorageScheduler::RunOperationIfIdle() {
  if (!running_operation_ && !pending_operations_.empty()) {
    // TODO(jkarlin): Run multiple operations in parallel where allowed.
    running_operation_ = std::move(pending_operations_.front());
    pending_operations_.pop_front();

    CACHE_STORAGE_SCHEDULER_UMA(
        TIMES, "QueueDuration", client_type_,
        base::TimeTicks::Now() - running_operation_->creation_ticks());

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&CacheStorageOperation::Run,
                                  running_operation_->AsWeakPtr()));
  }
}

}  // namespace content

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_operation.h"

#include "content/browser/cache_storage/cache_storage_histogram_utils.h"

namespace content {

namespace {
const int kNumSecondsForSlowOperation = 10;
}

CacheStorageOperation::CacheStorageOperation(
    base::OnceClosure closure,
    CacheStorageSchedulerId id,
    CacheStorageSchedulerClient client_type,
    CacheStorageSchedulerMode mode,
    CacheStorageSchedulerOp op_type,
    CacheStorageSchedulerPriority priority,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : closure_(std::move(closure)),
      creation_ticks_(base::TimeTicks::Now()),
      id_(id),
      client_type_(client_type),
      mode_(mode),
      op_type_(op_type),
      priority_(priority),
      task_runner_(std::move(task_runner)) {}

CacheStorageOperation::~CacheStorageOperation() {
  RecordCacheStorageSchedulerUMA(CacheStorageSchedulerUMA::kOperationDuration,
                                 client_type_, op_type_,
                                 base::TimeTicks::Now() - start_ticks_);

  if (!was_slow_)
    RecordCacheStorageSchedulerUMA(CacheStorageSchedulerUMA::kIsOperationSlow,
                                   client_type_, op_type_, was_slow_);
}

void CacheStorageOperation::Run() {
  start_ticks_ = base::TimeTicks::Now();

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CacheStorageOperation::NotifyOperationSlow,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kNumSecondsForSlowOperation));
  std::move(closure_).Run();
}

void CacheStorageOperation::NotifyOperationSlow() {
  was_slow_ = true;
  RecordCacheStorageSchedulerUMA(CacheStorageSchedulerUMA::kIsOperationSlow,
                                 client_type_, op_type_, was_slow_);
}

}  // namespace content

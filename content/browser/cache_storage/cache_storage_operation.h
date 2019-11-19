// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_OPERATION_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_OPERATION_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/cache_storage/cache_storage_scheduler_types.h"
#include "content/common/content_export.h"

namespace content {

// An operation to run in the CacheStorageScheduler. It's mostly just a closure
// to run plus a bunch of metrics data.
class CONTENT_EXPORT CacheStorageOperation {
 public:
  CacheStorageOperation(base::OnceClosure closure,
                        CacheStorageSchedulerId id,
                        CacheStorageSchedulerClient client_type,
                        CacheStorageSchedulerMode mode,
                        CacheStorageSchedulerOp op_type,
                        CacheStorageSchedulerPriority priority,
                        scoped_refptr<base::SequencedTaskRunner> task_runner);

  ~CacheStorageOperation();

  // Run the closure passed to the constructor.
  void Run();

  base::TimeTicks creation_ticks() const { return creation_ticks_; }
  CacheStorageSchedulerId id() const { return id_; }
  CacheStorageSchedulerMode mode() const { return mode_; }
  CacheStorageSchedulerOp op_type() const { return op_type_; }
  CacheStorageSchedulerPriority priority() const { return priority_; }
  base::WeakPtr<CacheStorageOperation> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void NotifyOperationSlow();

  // The operation's closure to run.
  base::OnceClosure closure_;

  // Ticks at time of object creation.
  base::TimeTicks creation_ticks_;

  // Ticks at time the operation's closure is run.
  base::TimeTicks start_ticks_;

  // If the operation took a long time to run.
  bool was_slow_ = false;

  const CacheStorageSchedulerId id_;
  const CacheStorageSchedulerClient client_type_;
  const CacheStorageSchedulerMode mode_;
  const CacheStorageSchedulerOp op_type_;
  const CacheStorageSchedulerPriority priority_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<CacheStorageOperation> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CacheStorageOperation);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_OPERATION_H_

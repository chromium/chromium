// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_SCHEDULER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_SCHEDULER_H_

#include <map>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/cache_storage/cache_storage_scheduler_types.h"
#include "content/common/content_export.h"

namespace content {

class CacheStorageOperation;

CONTENT_EXPORT BASE_DECLARE_FEATURE(kCacheStorageParallelOps);

// TODO(jkarlin): Support operation identification so that ops can be checked in
// DCHECKs.

// CacheStorageScheduler runs the scheduled callbacks sequentially. Add an
// operation by calling ScheduleOperation() with your callback. Once your
// operation is done be sure to call CompleteOperationAndRunNext() to schedule
// the next operation.
class CONTENT_EXPORT CacheStorageScheduler {
 public:
  // TODO(estade): remove `task_runner` which is invariably the same one that
  // `CacheStorageScheduler` is constructed and operated on (i.e. the "current
  // default").
  CacheStorageScheduler(CacheStorageSchedulerClient client_type,
                        scoped_refptr<base::SequencedTaskRunner> task_runner);

  CacheStorageScheduler(const CacheStorageScheduler&) = delete;
  CacheStorageScheduler& operator=(const CacheStorageScheduler&) = delete;

  virtual ~CacheStorageScheduler();

  // Create a scheduler-unique identifier for an operation to be scheduled.
  // This value must be passed to the ScheduleOperation(),
  // CompleteOperationAndRunNext(), and WrapCallbackToRunNext() methods.
  CacheStorageSchedulerId CreateId();

  // Adds the operation to the tail of the queue and starts it if possible.
  // A unique identifier must be provided via the CreateId() method.  The
  // mode determines whether the operation should run exclusively by itself
  // or can safely run in parallel with other shared operations. Note that the
  // operation may be executed either synchronously or asynchronously.
  void ScheduleOperation(CacheStorageSchedulerId id,
                         CacheStorageSchedulerMode mode,
                         CacheStorageSchedulerOp op_type,
                         CacheStorageSchedulerPriority priority,
                         base::OnceClosure closure);

  // Call this after each operation completes. It cleans up the operation
  // associated with the given id.  If may also start the next set of
  // operations.
  void CompleteOperationAndRunNext(CacheStorageSchedulerId id);

  // Returns true if there are any running or pending operations.
  bool ScheduledOperations() const;

  // Returns true if the scheduler is currently running an exclusive operation.
  bool IsRunningExclusiveOperation() const;

  // Wraps |callback| to also call CompleteOperationAndRunNext.
  template <typename... Args>
  base::OnceCallback<void(Args...)> WrapCallbackToRunNext(
      CacheStorageSchedulerId id,
      base::OnceCallback<void(Args...)> callback) {
    return base::BindOnce(&CacheStorageScheduler::RunNextContinuation<Args...>,
                          weak_ptr_factory_.GetWeakPtr(), id,
                          std::move(callback));
  }

 protected:
  // virtual for testing
  virtual void DispatchOperationTask(base::OnceClosure task);

 private:
  // Maybe start running the next operation depending on the current
  // set of running operations and the mode of the next operation.
  void MaybeRunOperation();

  template <typename... Args>
  void RunNextContinuation(CacheStorageSchedulerId id,
                           base::OnceCallback<void(Args...)> callback,
                           Args... args) {
    // Grab a weak ptr to guard against the scheduler being deleted during the
    // callback.
    base::WeakPtr<CacheStorageScheduler> scheduler =
        weak_ptr_factory_.GetWeakPtr();

    std::move(callback).Run(std::forward<Args>(args)...);
    if (scheduler)
      CompleteOperationAndRunNext(id);
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Managed as a heap using std::push_heap and std::pop_heap.  We do not
  // use std::priority_queue since it does not support moving the contained
  // unique_ptr out when the operation begins execution.
  std::vector<std::unique_ptr<CacheStorageOperation>> pending_operations_;

  std::map<CacheStorageSchedulerId, std::unique_ptr<CacheStorageOperation>>
      running_operations_;
  const CacheStorageSchedulerClient client_type_;
  CacheStorageSchedulerId next_id_ = 0;

  // Number of shared/exclusive operations currently running.
  int num_running_shared_ = 0;
  int num_running_exclusive_ = 0;

  // Whether the control flow is already inside `MaybeRunOperation()`. Used to
  // prevent recursion.
  bool in_maybe_run_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CacheStorageScheduler> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_SCHEDULER_H_

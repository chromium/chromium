// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_OP_SCHEDULER_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_OP_SCHEDULER_H_

#include <map>
#include <vector>

#include "base/containers/queue.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/common/content_export.h"

namespace content {

// BackgroundSyncOpScheduler runs the scheduled callbacks sequentially. Add an
// operation by calling ScheduleOperation() with your callback. Once your
// operation is done be sure to call CompleteOperationAndRunNext() to schedule
// the next operation.  This is a simplified version of CacheStorageScheduler.
class CONTENT_EXPORT BackgroundSyncOpScheduler {
 public:
  explicit BackgroundSyncOpScheduler(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  BackgroundSyncOpScheduler(const BackgroundSyncOpScheduler&) = delete;
  BackgroundSyncOpScheduler& operator=(const BackgroundSyncOpScheduler&) =
      delete;

  virtual ~BackgroundSyncOpScheduler();

  // Adds the operation to the tail of the queue and starts it if possible.
  void ScheduleOperation(base::OnceClosure closure);

  // Call this after each operation completes. It cleans up the operation
  // associated with the given id.  If may also start the next set of
  // operations.
  void CompleteOperationAndRunNext();

  // Returns true if there are any running or pending operations.
  bool ScheduledOperations() const;

  // Wraps |callback| to also call CompleteOperationAndRunNext.
  template <typename... Args>
  base::OnceCallback<void(Args...)> WrapCallbackToRunNext(
      base::OnceCallback<void(Args...)> callback) {
    return base::BindOnce(
        &BackgroundSyncOpScheduler::RunNextContinuation<Args...>,
        weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  }

 protected:
  // virtual for testing
  virtual void DoneStartingAvailableOperations() {}

 private:
  class Operation;

  // Maybe start running the next operation depending on the current
  // set of running operations and the mode of the next operation.
  void MaybeRunOperation();

  template <typename... Args>
  void RunNextContinuation(base::OnceCallback<void(Args...)> callback,
                           Args... args) {
    // Grab a weak ptr to guard against the scheduler being deleted during the
    // callback.
    base::WeakPtr<BackgroundSyncOpScheduler> scheduler =
        weak_ptr_factory_.GetWeakPtr();

    std::move(callback).Run(std::forward<Args>(args)...);
    if (scheduler)
      CompleteOperationAndRunNext();
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::queue<std::unique_ptr<Operation>> pending_operations_;

  std::unique_ptr<Operation> running_operation_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BackgroundSyncOpScheduler> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_OP_SCHEDULER_H_

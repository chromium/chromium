// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_EXCLUSIVE_OPERATION_COORDINATOR_H_
#define NET_DISK_CACHE_SQL_EXCLUSIVE_OPERATION_COORDINATOR_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <variant>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "net/base/net_export.h"
#include "net/disk_cache/sql/cache_entry_key.h"

namespace disk_cache {

// This class coordinates the execution of "normal" and "exclusive" operations
// to ensure that exclusive operations have exclusive access to a resource.
//
// - Normal operations are serialized by key. Operations with different keys can
//   run concurrently with each other.
// - Exclusive operations run one at a time, and only when no normal operations
//   are running.
// - When an exclusive operation is requested, it waits for all running normal
//   operations to complete.
// - While an exclusive operation is pending or running, any new normal
//   operations are queued and will only be executed after all pending
//   exclusive operations have finished.
class NET_EXPORT_PRIVATE ExclusiveOperationCoordinator {
 public:
  // An RAII-style handle that represents a running operation. The operation
  // is considered "finished" when this handle is destroyed. The destructor
  // notifies the coordinator to potentially start the next operation.
  // An operation is considered "exclusive" if its `key_` is `std::nullopt`, and
  // "normal" if it has a value.
  class NET_EXPORT_PRIVATE OperationHandle {
   public:
    OperationHandle(base::PassKey<ExclusiveOperationCoordinator>,
                    base::WeakPtr<ExclusiveOperationCoordinator> coordinator,
                    std::optional<CacheEntryKey> key);
    ~OperationHandle();

    OperationHandle(const OperationHandle&) = delete;
    OperationHandle& operator=(const OperationHandle&) = delete;
    OperationHandle(OperationHandle&&) = delete;
    OperationHandle& operator=(OperationHandle&&) = delete;

   private:
    base::WeakPtr<ExclusiveOperationCoordinator> coordinator_;
    const std::optional<CacheEntryKey> key_;
  };

  using OperationCallback =
      base::OnceCallback<void(std::unique_ptr<OperationHandle>)>;

  ExclusiveOperationCoordinator();
  ~ExclusiveOperationCoordinator();

  ExclusiveOperationCoordinator(const ExclusiveOperationCoordinator&) = delete;
  ExclusiveOperationCoordinator& operator=(
      const ExclusiveOperationCoordinator&) = delete;
  ExclusiveOperationCoordinator(ExclusiveOperationCoordinator&&) = delete;
  ExclusiveOperationCoordinator& operator=(ExclusiveOperationCoordinator&&) =
      delete;

  // Posts an exclusive operation. The operation will be executed after all
  // currently running normal operations have completed. While this and any
  // other exclusive operations are pending or running, no new normal
  // operations will start.
  //
  // If `low_priority` is false, this operation is considered a "pending task"
  // by `GetHasPendingTaskFlag()` while it is waiting in the queue.
  void PostOrRunExclusiveOperation(OperationCallback operation,
                                   bool low_priority = false);

  // Posts a normal operation. If no exclusive operations are pending or
  // running, the operation is executed immediately. Otherwise, it is queued
  // and will be executed after all exclusive operations have finished. This
  // operation will be serialized with other normal operations that have the
  // same `key`.
  //
  // If `low_priority` is false, this operation is considered a "pending task"
  // by `GetHasPendingTaskFlag()` while it is waiting in the queue.
  void PostOrRunNormalOperation(const CacheEntryKey& key,
                                OperationCallback operation,
                                bool low_priority = false);

  // Returns a flag that indicates whether there are any "pending" tasks.
  // A task is considered "pending" if it has been posted but has not yet
  // started running, and `low_priority` was false when it was posted.
  //
  // This flag can be used to detect if any pending tasks have been posted,
  // allowing an operation (e.g., eviction) to potentially abort or adjust its
  // behavior.
  //
  // Note: This flag is shared with operations via `RefCountedData` so they can
  // check it without holding a reference to the coordinator. Since
  // `RefCountedData` is `RefCountedThreadSafe` and the data is `std::atomic`,
  // the state can be safely referenced from other sequences.
  const scoped_refptr<base::RefCountedData<std::atomic_bool>>&
  GetHasPendingTaskFlag() {
    return has_pending_task_;
  }


 private:
  using NormalOperationsQueueMap =
      std::map<CacheEntryKey, base::queue<OperationCallback>>;
  using ExclusiveOperation = OperationCallback;
  using NormalOperationsQueueMapOrExclusiveOperation =
      std::variant<NormalOperationsQueueMap, ExclusiveOperation>;

  // Called by OperationHandle's destructor. `key` has a value for a normal
  // operation, and is `std::nullopt` for an exclusive operation.
  void OnOperationFinished(const std::optional<CacheEntryKey>& key);

  // Checks the current state and runs the next appropriate operation. `key` has
  // a value if a normal operation was posted or finished, and is `std::nullopt`
  // if an exclusive operation was posted or finished.
  void TryToRunNextOperation(const std::optional<CacheEntryKey>& key);

  // Prepares a pending `operation` for execution if it is not already running.
  // The `operation` is moved into a `base::OnceClosure`, bound with an
  // `OperationHandle`, and added to `runnable_ops`. The original `operation`
  // callback is reset to a null state to mark it as in-flight.
  void MaybeTakeAndResetPendingOperation(
      OperationCallback& operation,
      const std::optional<CacheEntryKey>& key,
      std::vector<base::OnceClosure>& runnable_ops);

  // Increments `pending_task_count_` and updates `has_pending_task_` if
  // `low_priority` is false. Returns a `ScopedClosureRunner` that will
  // decrement the count when it goes out of scope.
  base::ScopedClosureRunner MaybeIncrementPendingTaskCount(bool low_priority);

  // Decrements `pending_task_count_` and updates `has_pending_task_` if
  // the count reaches 0. This is called when a pending task starts running.
  void DecrementPendingTaskCount();

  // A queue of operation "phases". Each element is either a
  // `NormalOperationsQueueMap` (a batch of normal operations) or a single
  // `ExclusiveOperation`. This structure enforces the serialization between
  // normal and exclusive operations. For example, an exclusive operation will
  // only run after all operations in the preceding `NormalOperationsQueueMap`
  // batch have completed. Normal operations that arrive while an exclusive
  // operation is pending are added to a new batch that runs after the exclusive
  // operation completes.
  base::queue<NormalOperationsQueueMapOrExclusiveOperation> queue_;

  // See `GetHasPendingTaskFlag()` for details.
  scoped_refptr<base::RefCountedData<std::atomic_bool>> has_pending_task_;

  // The number of pending tasks in the queue. Does not include running tasks
  // or tasks posted with `low_priority=true`.
  int64_t pending_task_count_ = 0;

  base::WeakPtrFactory<ExclusiveOperationCoordinator> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_EXCLUSIVE_OPERATION_COORDINATOR_H_

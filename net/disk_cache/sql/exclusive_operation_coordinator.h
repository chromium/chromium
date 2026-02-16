// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_EXCLUSIVE_OPERATION_COORDINATOR_H_
#define NET_DISK_CACHE_SQL_EXCLUSIVE_OPERATION_COORDINATOR_H_

#include <atomic>
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
  // Note: An exclusive operation that is currently running is NOT considered a
  // "pending task" by `GetHasPendingTaskFlag()`.
  void PostOrRunExclusiveOperation(OperationCallback operation);

  // Posts a normal operation. If no exclusive operations are pending or
  // running, the operation is executed immediately. Otherwise, it is queued
  // and will be executed after all exclusive operations have finished. This
  // operation will be serialized with other normal operations that have the
  // same `key`.
  //
  // Note: A normal operation that is currently running IS considered a
  // "pending task" by `GetHasPendingTaskFlag()`.
  void PostOrRunNormalOperation(const CacheEntryKey& key,
                                OperationCallback operation);

  // Returns a flag that indicates whether there are any "pending" tasks.
  // A "pending" task is defined as:
  // - Any normal operation (whether running or waiting).
  // - Any *waiting* exclusive operation (i.e. not the one currently running).
  //
  // Crucially, a single running exclusive operation is NOT considered pending.
  // This flag is used during the execution of an `ExclusiveOperation` (e.g.,
  // eviction) to detect if any other tasks have been posted, allowing the
  // operation to potentially abort or adjust its behavior.
  //
  // Note: This flag is shared with operations via `RefCountedData` so they can
  // check it without holding a reference to the coordinator. Since
  // `RefCountedData` is `RefCountedThreadSafe` and the data is `std::atomic`,
  // the state can be safely referenced from other sequences.
  const scoped_refptr<base::RefCountedData<std::atomic_bool>>&
  GetHasPendingTaskFlag() {
    return has_pending_task_;
  }

  // Forces `GetHasPendingTaskFlag()` to return false as long as the returned
  // `ScopedClosureRunner` is alive. This is used when posting an
  // ExclusiveOperation to wait for all pending tasks to complete, preventing
  // `has_pending_task_` from being set to true which might trigger unwanted
  // side effects (e.g. aborting an eviction logic).
  base::ScopedClosureRunner KeepHasPendingTaskFlagUnsetForTesting();

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

  // Updates the `has_pending_task_` flag based on the current state of the
  // queue.
  void UpdateHasPendingTaskFlag();

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

  // A counter used by tests to forcibly keep `has_pending_task_` false.
  // When this count is greater than 0, `UpdateHasPendingTaskFlag` will
  // always set `has_pending_task_` to false.
  int keep_has_pending_task_unset_count_ = 0;

  base::WeakPtrFactory<ExclusiveOperationCoordinator> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_EXCLUSIVE_OPERATION_COORDINATOR_H_

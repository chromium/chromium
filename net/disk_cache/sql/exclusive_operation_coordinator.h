// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_EXCLUSIVE_OPERATION_COORDINATOR_H_
#define NET_DISK_CACHE_SQL_EXCLUSIVE_OPERATION_COORDINATOR_H_

#include <map>
#include <memory>
#include <optional>
#include <set>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
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
  void PostOrRunExclusiveOperation(
      base::OnceCallback<void(std::unique_ptr<OperationHandle>)> operation);

  // Posts a normal operation. If no exclusive operations are pending or
  // running, the operation is executed immediately. Otherwise, it is queued
  // and will be executed after all exclusive operations have finished. This
  // operation will be serialized with other normal operations that have the
  // same `key`.
  void PostOrRunNormalOperation(
      const CacheEntryKey& key,
      base::OnceCallback<void(std::unique_ptr<OperationHandle>)> operation);

 private:
  // Called by OperationHandle's destructor. `key` has a value for a normal
  // operation, and is `std::nullopt` for an exclusive operation.
  void OnOperationFinished(const std::optional<CacheEntryKey>& key);

  // Checks the current state and runs the next appropriate operation. `key` has
  // a value if a normal operation has just finished, and is `std::nullopt` if
  // an exclusive operation has just finished or if no operation has finished.
  void TryToRunNextOperation(const std::optional<CacheEntryKey>& key);

  // Runs pending normal operations. If `key` has a value, only operations for
  // that key are considered. Otherwise, all pending normal operations are.
  void RunPendingNormalOperations(const std::optional<CacheEntryKey>& key);

  // Tries to run a pending normal operation for `key`. If an operation can be
  // run, it is added to `runnable_ops`. Returns true if the queue for `key` is
  // now empty and can be erased from `pending_normal_operations_`.
  bool TryToRunNormalOperationForKey(
      const CacheEntryKey& key,
      std::queue<base::OnceCallback<void(std::unique_ptr<OperationHandle>)>>&
          queue,
      std::vector<base::OnceClosure>& runnable_ops);

  // True if an exclusive operation is currently running.
  bool exclusive_operation_running_ = false;

  // A queue of exclusive operations waiting to be run.
  base::queue<base::OnceCallback<void(std::unique_ptr<OperationHandle>)>>
      pending_exclusive_operations_;

  // The set of keys for which a normal operation is currently running.
  std::set<CacheEntryKey> running_normal_operations_;

  // A map from a key to a queue of pending normal operations for that key.
  std::map<
      CacheEntryKey,
      std::queue<base::OnceCallback<void(std::unique_ptr<OperationHandle>)>>>
      pending_normal_operations_;

  base::WeakPtrFactory<ExclusiveOperationCoordinator> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_EXCLUSIVE_OPERATION_COORDINATOR_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/exclusive_operation_coordinator.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "net/disk_cache/sql/cache_entry_key.h"

namespace disk_cache {

namespace {

// Wraps an operation to record its queuing time in a UMA histogram.
base::OnceCallback<
    void(std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle>)>
WrapWithUmaQueuingTime(
    base::OnceCallback<
        void(std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle>)>
        operation,
    const std::string_view histogram_name) {
  return base::BindOnce(
      [](base::OnceCallback<void(
             std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle>)>
             operation,
         const std::string_view histogram_name, base::ElapsedTimer timer,
         std::unique_ptr<ExclusiveOperationCoordinator::OperationHandle>
             handle) {
        base::UmaHistogramMicrosecondsTimes(histogram_name, timer.Elapsed());
        std::move(operation).Run(std::move(handle));
      },
      std::move(operation), histogram_name, base::ElapsedTimer());
}

}  // namespace

ExclusiveOperationCoordinator::OperationHandle::OperationHandle(
    base::PassKey<ExclusiveOperationCoordinator>,
    base::WeakPtr<ExclusiveOperationCoordinator> coordinator,
    std::optional<CacheEntryKey> key)
    : coordinator_(std::move(coordinator)), key_(std::move(key)) {}

ExclusiveOperationCoordinator::OperationHandle::~OperationHandle() {
  if (coordinator_) {
    coordinator_->OnOperationFinished(key_);
  }
}

ExclusiveOperationCoordinator::ExclusiveOperationCoordinator() = default;
ExclusiveOperationCoordinator::~ExclusiveOperationCoordinator() = default;

void ExclusiveOperationCoordinator::PostOrRunExclusiveOperation(
    OperationCallback operation) {
  CHECK(operation);
  operation = WrapWithUmaQueuingTime(
      std::move(operation), "Net.SqlDiskCache.ExclusiveOperationDelay");
  queue_.emplace(std::move(operation));
  TryToRunNextOperation(std::nullopt);
}

void ExclusiveOperationCoordinator::PostOrRunNormalOperation(
    const CacheEntryKey& key,
    OperationCallback operation) {
  CHECK(operation);
  operation = WrapWithUmaQueuingTime(std::move(operation),
                                     "Net.SqlDiskCache.NormalOperationDelay");
  // If there is no queue, or the back of the queue is an exclusive operation,
  // add a new `NormalOperationsQueueMap` to the back of the queue.
  if (queue_.empty() ||
      std::holds_alternative<ExclusiveOperation>(queue_.back())) {
    queue_.push(NormalOperationsQueueMap());
  }
  // Add the callback to the queue for the given `key`.
  // Normal operations with the same key are serialized.
  std::get<NormalOperationsQueueMap>(queue_.back())[key].push(
      std::move(operation));
  TryToRunNextOperation(key);
}

void ExclusiveOperationCoordinator::OnOperationFinished(
    const std::optional<CacheEntryKey>& key) {
  CHECK(!queue_.empty());
  // Verify that the front of the queue is a `NormalOperationsQueueMap` iff a
  // `key` was provided.
  CHECK_EQ(std::holds_alternative<NormalOperationsQueueMap>(queue_.front()),
           key.has_value());

  if (key.has_value()) {
    // The operation that just finished was a normal operation.
    // Get a reference to the `NormalOperationsQueueMap` at the front of the
    // queue.
    NormalOperationsQueueMap& normal_operations_map =
        std::get<NormalOperationsQueueMap>(queue_.front());
    auto it = normal_operations_map.find(key.value());
    // The `NormalOperationsQueueMap` at the front of `queue_` must have a
    // `base::queue<OperationCallback>` corresponding to `key`,
    CHECK(it != normal_operations_map.end());
    // and that `base::queue<OperationCallback>` must not be empty,
    CHECK(!it->second.empty());
    // and the OperationCallback at the front must be a null callback.
    CHECK(it->second.front().is_null());
    // Remove the OperationCallback that just finished running.
    it->second.pop();
    if (it->second.empty()) {
      // There are no more operations for this key, so remove the key from the
      // map.
      normal_operations_map.erase(it);
      if (normal_operations_map.empty()) {
        // There are no more operations in the map, so remove the map from the
        // queue. This phase has completed.
        queue_.pop();
      }
    }
  } else {
    // The operation that just finished was an exclusive operation.
    // Get a reference to the `ExclusiveOperation` at the front of the queue.
    ExclusiveOperation& exclusive_operation =
        std::get<ExclusiveOperation>(queue_.front());
    // The ExclusiveOperation at the front of `queue_` must be a null callback.
    CHECK(exclusive_operation.is_null());
    // Remove the ExclusiveOperation from the queue. This phase has completed.
    queue_.pop();
  }

  // The completion of an operation might allow the next one to start.
  TryToRunNextOperation(key);
}

void ExclusiveOperationCoordinator::TryToRunNextOperation(
    const std::optional<CacheEntryKey>& key) {
  if (queue_.empty()) {
    // Nothing to do.
    return;
  }

  // A list of operations that can be run in this pass. We collect them first
  // and run them later to avoid iterator invalidation issues caused by
  // re-entrant calls if an operation completes synchronously.
  std::vector<base::OnceClosure> runnable_ops;

  if (std::holds_alternative<NormalOperationsQueueMap>(queue_.front())) {
    // The next phase in the queue is a batch of normal operations.
    // Get a reference to the `NormalOperationsQueueMap` at the front of the
    // queue.
    NormalOperationsQueueMap& normal_operations_map =
        std::get<NormalOperationsQueueMap>(queue_.front());
    // If a `key` was provided, attempt to run the next operation for that
    // `key`. Otherwise, attempt to run operations for all keys.
    if (key.has_value()) {
      if (auto it = normal_operations_map.find(key.value());
          it != normal_operations_map.end()) {
        CHECK(!it->second.empty());
        MaybeTakeAndResetPendingOperation(it->second.front(), key,
                                          runnable_ops);
      }
    } else {
      // Attempt to run one operation for each key in the map.
      for (auto& pair : normal_operations_map) {
        CHECK(!pair.second.empty());
        MaybeTakeAndResetPendingOperation(pair.second.front(), pair.first,
                                          runnable_ops);
      }
    }
  } else {
    // The next phase in the queue is an exclusive operation.
    MaybeTakeAndResetPendingOperation(
        std::get<ExclusiveOperation>(queue_.front()), std::nullopt,
        runnable_ops);
  }

  // Run the collected operations.
  for (auto& runnable_op : runnable_ops) {
    std::move(runnable_op).Run();
  }
}

void ExclusiveOperationCoordinator::MaybeTakeAndResetPendingOperation(
    ExclusiveOperationCoordinator::OperationCallback& operation,
    const std::optional<CacheEntryKey>& key,
    std::vector<base::OnceClosure>& runnable_ops) {
  if (!operation) {
    return;
  }
  runnable_ops.push_back(base::BindOnce(
      std::move(operation), std::make_unique<OperationHandle>(
                                base::PassKey<ExclusiveOperationCoordinator>(),
                                weak_factory_.GetWeakPtr(), key)));
  // Reset to a null callback to indicate that the operation is
  // currently running.
  operation.Reset();
}

}  // namespace disk_cache

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
    base::OnceCallback<void(std::unique_ptr<OperationHandle>)> operation) {
  operation = WrapWithUmaQueuingTime(
      std::move(operation), "Net.SqlDiskCache.ExclusiveOperationDelay");
  pending_exclusive_operations_.push(std::move(operation));
  TryToRunNextOperation(std::nullopt);
}

void ExclusiveOperationCoordinator::PostOrRunNormalOperation(
    const CacheEntryKey& key,
    base::OnceCallback<void(std::unique_ptr<OperationHandle>)> operation) {
  operation = WrapWithUmaQueuingTime(std::move(operation),
                                     "Net.SqlDiskCache.NormalOperationDelay");
  // If an exclusive operation is running or pending, queue the normal
  // operation. It will be run after all exclusive operations are done.
  // TODO(crbug.com/422065015): The current implementation prioritizes
  // exclusive operations, which could potentially starve normal operations if
  // exclusive operations are frequent. If the delay is unacceptable, we may
  // need to implement a more sophisticated scheduling mechanism to ensure
  // fairness.
  if (exclusive_operation_running_ || !pending_exclusive_operations_.empty()) {
    pending_normal_operations_[key].push(std::move(operation));
    return;
  }

  if (running_normal_operations_.count(key)) {
    // An operation for this key is already running, so queue this one.
    pending_normal_operations_[key].push(std::move(operation));
    return;
  }

  // Otherwise, run the normal operation immediately.
  running_normal_operations_.insert(key);
  std::move(operation).Run(std::make_unique<OperationHandle>(
      base::PassKey<ExclusiveOperationCoordinator>(),
      weak_factory_.GetWeakPtr(), key));
}

void ExclusiveOperationCoordinator::OnOperationFinished(
    const std::optional<CacheEntryKey>& key) {
  if (key.has_value()) {
    // A normal operation has finished.
    CHECK(running_normal_operations_.count(key.value()));
    running_normal_operations_.erase(key.value());
  } else {
    // An exclusive operation has finished.
    CHECK(exclusive_operation_running_);
    exclusive_operation_running_ = false;
  }

  // The completion of an operation might allow the next one to start.
  TryToRunNextOperation(key);
}

void ExclusiveOperationCoordinator::TryToRunNextOperation(
    const std::optional<CacheEntryKey>& key) {
  // An exclusive operation is already running. Let its handle's destruction
  // trigger the next operation.
  if (exclusive_operation_running_) {
    return;
  }

  // If there are pending exclusive operations, try to run one.
  if (!pending_exclusive_operations_.empty()) {
    // Wait for all currently active normal operations to complete before
    // starting an exclusive one.
    if (!running_normal_operations_.empty()) {
      return;
    }

    // All conditions met, run the next exclusive operation.
    exclusive_operation_running_ = true;
    auto operation = std::move(pending_exclusive_operations_.front());
    pending_exclusive_operations_.pop();
    std::move(operation).Run(std::make_unique<OperationHandle>(
        base::PassKey<ExclusiveOperationCoordinator>(),
        weak_factory_.GetWeakPtr(), std::nullopt));
    return;
  }

  // No exclusive operations are pending or running. Run any pending normal
  // operations.
  RunPendingNormalOperations(key);
}

void ExclusiveOperationCoordinator::RunPendingNormalOperations(
    const std::optional<CacheEntryKey>& key) {
  // This should only be called when no exclusive operations are running or
  // pending.
  CHECK(!exclusive_operation_running_);
  CHECK(pending_exclusive_operations_.empty());

  // A list of operations that can be run in this pass. We collect them first
  // and run them later to avoid iterator invalidation issues caused by
  // re-entrant calls if an operation completes synchronously.
  std::vector<base::OnceClosure> runnable_ops;

  if (key.has_value()) {
    // A normal operation finished. We only need to check its key.
    auto it = pending_normal_operations_.find(key.value());
    if (it != pending_normal_operations_.end()) {
      if (TryToRunNormalOperationForKey(it->first, it->second, runnable_ops)) {
        pending_normal_operations_.erase(it);
      }
    }
  } else {
    // An exclusive operation finished. Check all keys.
    for (auto it = pending_normal_operations_.begin();
         it != pending_normal_operations_.end();) {
      if (TryToRunNormalOperationForKey(it->first, it->second, runnable_ops)) {
        it = pending_normal_operations_.erase(it);
      } else {
        ++it;
      }
    }
  }

  // Run the collected operations.
  for (auto& runnable_op : runnable_ops) {
    std::move(runnable_op).Run();
  }
}

bool ExclusiveOperationCoordinator::TryToRunNormalOperationForKey(
    const CacheEntryKey& key,
    std::queue<base::OnceCallback<void(std::unique_ptr<OperationHandle>)>>&
        queue,
    std::vector<base::OnceClosure>& runnable_ops) {
  if (running_normal_operations_.count(key) == 0 && !queue.empty()) {
    running_normal_operations_.insert(key);
    auto operation = std::move(queue.front());
    queue.pop();
    runnable_ops.push_back(
        base::BindOnce(std::move(operation),
                       std::make_unique<OperationHandle>(
                           base::PassKey<ExclusiveOperationCoordinator>(),
                           weak_factory_.GetWeakPtr(), key)));
  }
  return queue.empty();
}

}  // namespace disk_cache

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_observer_quota_manager.h"

#include "base/metrics/histogram_functions.h"
#include "content/browser/file_system_access/file_system_access_watcher_manager.h"

namespace content {

FileSystemAccessObserverQuotaManager::FileSystemAccessObserverQuotaManager(
    const blink::StorageKey& storage_key,
    FileSystemAccessWatcherManager& watcher_manager)
    : base::RefCountedDeleteOnSequence<FileSystemAccessObserverQuotaManager>(
          base::SequencedTaskRunner::GetCurrentDefault()),
      storage_key_(storage_key),
      watcher_manager_(watcher_manager) {}

// TODO(crbug.com/338457523): Inform the watcher manager to remove this
// from the quota manager map entry for this storage key.
FileSystemAccessObserverQuotaManager::~FileSystemAccessObserverQuotaManager() {
  CHECK(quota_limit_ > 0);
  base::UmaHistogramCounts100000("Storage.FileSystemAccess.ObserverUsage",
                                 high_water_mark_usage_);
  base::UmaHistogramPercentage("Storage.FileSystemAccess.ObserverUsageRate",
                               100 * high_water_mark_usage_ / quota_limit_);
  base::UmaHistogramBoolean(
      "Storage.FileSystemAccess.ObserverUsageQuotaExceeded",
      reached_quota_limit_);

  watcher_manager_->RemoveQuotaManager(storage_key_);
}

FileSystemAccessObserverQuotaManager::UsageChangeResult
FileSystemAccessObserverQuotaManager::OnUsageChange(size_t old_usage,
                                                    size_t new_usage) {
  // The caller should have reported this `old_usage` in its last call, so that
  // `total_usage_` is equal to the sum of `old_usage` plus possibly other
  // observation group usages.
  CHECK_GE(total_usage_, old_usage);

  size_t updated_total_usage = total_usage_ + new_usage - old_usage;
  if (updated_total_usage > quota_limit_) {
    total_usage_ -= old_usage;
    reached_quota_limit_ = true;
    return UsageChangeResult::kQuotaUnavailable;
  }

  if (updated_total_usage > high_water_mark_usage_) {
    high_water_mark_usage_ = updated_total_usage;
  }
  total_usage_ = updated_total_usage;
  return UsageChangeResult::kOk;
}

}  // namespace content

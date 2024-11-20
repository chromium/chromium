// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_observer_quota_manager.h"

namespace content {

FileSystemAccessObserverQuotaManager::FileSystemAccessObserverQuotaManager(
    const blink::StorageKey& storage_key,
    FileSystemAccessWatcherManager* watcher_manager)
    : base::RefCountedDeleteOnSequence<FileSystemAccessObserverQuotaManager>(
          base::SequencedTaskRunner::GetCurrentDefault()),
      storage_key_(storage_key),
      watcher_manager_(watcher_manager) {}

// TODO(crbug.com/338457523): Inform the watcher manager to remove this
// from the quota manager map entry for this storage key.
// TODO(crbug.com/338457523): Report metrics.
FileSystemAccessObserverQuotaManager::~FileSystemAccessObserverQuotaManager() =
    default;

FileSystemAccessObserverQuotaManager::UsageChangeResult

FileSystemAccessObserverQuotaManager::OnUsageChange(size_t old_usage,
                                                    size_t new_usage) {
  // The caller should have reported this `old_usage` in its last call, so that
  // `total_usage_` is equal to the sum of `old_usage` plus possibly other
  // observation group usages.
  CHECK_GE(total_usage_, static_cast<int64_t>(old_usage));

  int64_t updated_total_usage = total_usage_ + new_usage - old_usage;

  // TODO(crbug.com/338457523): Use FileSystemAccessChangeSource::quota_limit()
  // once the implementation is ready.
  if (quota_limit_for_testing_ > 0 &&
      updated_total_usage > quota_limit_for_testing_) {
    total_usage_ -= old_usage;
    return UsageChangeResult::kQuotaUnavailable;
  }

  total_usage_ = updated_total_usage;
  return UsageChangeResult::kOk;
}

}  // namespace content

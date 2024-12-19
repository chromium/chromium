// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_observer_quota_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/file_system_access/file_system_access_watcher_manager.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content {
FileSystemAccessObserverQuotaManager::Handle::Handle(
    scoped_refptr<FileSystemAccessObserverQuotaManager> quota_manager)
    : quota_manager_(std::move(quota_manager)) {}

FileSystemAccessObserverQuotaManager::Handle::~Handle() = default;

FileSystemAccessObserverQuotaManager::Handle::Handle(Handle&&) = default;
FileSystemAccessObserverQuotaManager::Handle&
FileSystemAccessObserverQuotaManager::Handle::operator=(Handle&&) = default;

FileSystemAccessObserverQuotaManager::UsageChangeResult
FileSystemAccessObserverQuotaManager::Handle::OnUsageChange(size_t usage) {
  if (errored_) {
    return UsageChangeResult::kQuotaUnavailable;
  }

  UsageChangeResult result = quota_manager_->OnUsageChange(old_usage_, usage);

  old_usage_ = usage;
  errored_ = result == UsageChangeResult::kQuotaUnavailable;

  return result;
}

FileSystemAccessObserverQuotaManager::FileSystemAccessObserverQuotaManager(
    const blink::StorageKey& storage_key,
    ukm::SourceId ukm_source_id,
    FileSystemAccessWatcherManager& watcher_manager)
    : base::RefCountedDeleteOnSequence<FileSystemAccessObserverQuotaManager>(
          base::SequencedTaskRunner::GetCurrentDefault()),
      storage_key_(storage_key),
      ukm_source_id_(ukm_source_id),
      watcher_manager_(watcher_manager) {}

FileSystemAccessObserverQuotaManager::~FileSystemAccessObserverQuotaManager() {
  CHECK(FileSystemAccessChangeSource::quota_limit() > 0);
  // The percentile value, rounded down to the nearest integer.
  size_t usage_rate = 100 * high_water_mark_usage_ /
                      FileSystemAccessChangeSource::quota_limit();

  // UMA logging.
  if (high_water_mark_usage_ > 0) {
    base::UmaHistogramCounts100000("Storage.FileSystemAccess.ObserverUsage",
                                   high_water_mark_usage_);
    base::UmaHistogramPercentage("Storage.FileSystemAccess.ObserverUsageRate",
                                 usage_rate);
  }
  base::UmaHistogramBoolean(
      "Storage.FileSystemAccess.ObserverUsageQuotaExceeded",
      reached_quota_limit_);

  // UKM logging.
  if (ukm_source_id_ != ukm::kInvalidSourceId) {
    auto ukm_builder = ukm::builders::FileSystemObserver_Usage(ukm_source_id_);
    if (high_water_mark_usage_ > 0) {
      ukm_builder
          .SetHighWaterMark(ukm::GetExponentialBucketMin(
              high_water_mark_usage_, kHighWaterMarkBucketSpacing))
          .SetHighWaterMarkPercentage(usage_rate);
    }
    ukm_builder.SetQuotaExceeded(reached_quota_limit_)
        .Record(ukm::UkmRecorder::Get());
  }

  watcher_manager_->RemoveQuotaManager(storage_key_);
}

FileSystemAccessObserverQuotaManager::Handle
FileSystemAccessObserverQuotaManager::CreateHandle() {
  return Handle(base::WrapRefCounted(this));
}

FileSystemAccessObserverQuotaManager::UsageChangeResult
FileSystemAccessObserverQuotaManager::OnUsageChange(size_t old_usage,
                                                    size_t new_usage) {
  // The caller should have reported this `old_usage` in its last call, so that
  // `total_usage_` is equal to the sum of `old_usage` plus possibly other
  // observation group usages.
  CHECK_GE(total_usage_, old_usage);

  size_t updated_total_usage = total_usage_ + new_usage - old_usage;
  if (updated_total_usage > FileSystemAccessChangeSource::quota_limit()) {
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

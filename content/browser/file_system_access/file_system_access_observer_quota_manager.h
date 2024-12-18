// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVER_QUOTA_MANAGER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVER_QUOTA_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "content/browser/file_system_access/file_system_access_change_source.h"
#include "content/common/content_export.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

class FileSystemAccessWatcherManager;

// Keeps track of the total usage of observer resource for a given StorageKey.
// TODO(crbug.com/338457523): Update size_t to int64_t.
class CONTENT_EXPORT FileSystemAccessObserverQuotaManager
    : public base::RefCountedDeleteOnSequence<
          FileSystemAccessObserverQuotaManager> {
 public:
  enum UsageChangeResult {
    kOk,
    kQuotaUnavailable,
  };

  explicit FileSystemAccessObserverQuotaManager(
      const blink::StorageKey& storage_key,
      ukm::SourceId ukm_source_id,
      FileSystemAccessWatcherManager& watcher_manager);

  // Updates the total usage if the quota is available.
  // Otherwise, returns `UsageChangeResult::kQuotaUnavailable`.
  //
  // The first call to it must always have an `old_usage` of zero. Subsequent
  // calls must use `new_usage` of the last call as their `old_usage`.
  // A caller should not call this again if it receives `kQuotaUnavailable`.
  UsageChangeResult OnUsageChange(size_t old_usage, size_t new_usage);

  void SetQuotaLimitForTesting(size_t quota_limit) {
    CHECK(quota_limit > 0);
    quota_limit_ = quota_limit;
  }

  size_t GetTotalUsageForTesting() { return total_usage_; }

  // Since what OS resource represents differs by platform, the bucket sizing
  // would result in different # of buckets. The bucket spacing of 1.1 is
  // chosen to give minimum of ~50 bucket size on all platforms (i.e. the lowest
  // number of buckets 49 on Mac): x^(1/n) = 1.1, where x is the expected max
  // quota limit, and n is the number of buckets.
  //   Windows: 211, where x is 1/2 GiB
  //   Mac: 49, where x is 512*0.2
  //   Linux: 145, where x is 1,000,000
  static constexpr double kHighWaterMarkBucketSpacing = 1.1;

 private:
  friend class base::RefCountedDeleteOnSequence<
      FileSystemAccessObserverQuotaManager>;
  friend class base::DeleteHelper<FileSystemAccessObserverQuotaManager>;
  ~FileSystemAccessObserverQuotaManager();

  const blink::StorageKey storage_key_;
  ukm::SourceId ukm_source_id_;

  const raw_ref<FileSystemAccessWatcherManager> watcher_manager_;
  // OS-specific quota limit. Must be greater than 0.
  size_t quota_limit_ = FileSystemAccessChangeSource::quota_limit();
  size_t total_usage_ = 0;
  size_t high_water_mark_usage_ = 0;
  bool reached_quota_limit_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVER_QUOTA_MANAGER_H_

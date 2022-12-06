// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_STORAGE_BUCKET_CLEAR_SITE_DATA_TESTER_H_
#define CONTENT_BROWSER_BROWSING_DATA_STORAGE_BUCKET_CLEAR_SITE_DATA_TESTER_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

class StoragePartitionImpl;

class StorageBucketClearSiteDataTester {
 public:
  explicit StorageBucketClearSiteDataTester(
      StoragePartition* storage_partition);

  void CreateBucketForTesting(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      base::OnceCallback<void(storage::QuotaErrorOr<storage::BucketInfo>)>
          callback);

  void GetBucketsForStorageKey(
      const blink::StorageKey& storage_key,
      base::OnceCallback<
          void(storage::QuotaErrorOr<std::set<storage::BucketInfo>>)> callback);

 private:
  raw_ptr<StoragePartitionImpl> storage_partition_impl_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_DATA_STORAGE_BUCKET_CLEAR_SITE_DATA_TESTER_H_

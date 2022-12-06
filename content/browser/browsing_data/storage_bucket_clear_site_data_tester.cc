// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/storage_bucket_clear_site_data_tester.h"

namespace content {

StorageBucketClearSiteDataTester::StorageBucketClearSiteDataTester(
    StoragePartition* storage_partition)
    : storage_partition_impl_(
          static_cast<StoragePartitionImpl*>(storage_partition)) {}

void StorageBucketClearSiteDataTester::CreateBucketForTesting(
    const blink::StorageKey& storage_key,
    const std::string& bucket_name,
    base::OnceCallback<void(storage::QuotaErrorOr<storage::BucketInfo>)>
        callback) {
  storage_partition_impl_->GetQuotaManagerProxy()
      ->CreateBucketForTesting(  // IN-TEST
          storage_key, bucket_name, blink::mojom::StorageType::kTemporary,
          base::SequencedTaskRunnerHandle::Get(), std::move(callback));
}

void StorageBucketClearSiteDataTester::GetBucketsForStorageKey(
    const blink::StorageKey& storage_key,
    base::OnceCallback<
        void(storage::QuotaErrorOr<std::set<storage::BucketInfo>>)> callback) {
  storage_partition_impl_->GetQuotaManagerProxy()->GetBucketsForStorageKey(
      storage_key, blink::mojom::StorageType::kTemporary,
      /*delete_expired=*/false, base::SequencedTaskRunnerHandle::Get(),
      std::move(callback));
}

}  // namespace content

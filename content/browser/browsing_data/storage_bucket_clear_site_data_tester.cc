// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/storage_bucket_clear_site_data_tester.h"

#include <set>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

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
          base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback));
}

void StorageBucketClearSiteDataTester::GetBucketsForStorageKey(
    const blink::StorageKey& storage_key,
    base::OnceCallback<
        void(storage::QuotaErrorOr<std::set<storage::BucketInfo>>)> callback) {
  storage_partition_impl_->GetQuotaManagerProxy()->GetBucketsForStorageKey(
      storage_key, blink::mojom::StorageType::kTemporary,
      /*delete_expired=*/false, base::SequencedTaskRunner::GetCurrentDefault(),
      std::move(callback));
}

}  // namespace content

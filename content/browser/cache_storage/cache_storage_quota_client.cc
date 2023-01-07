// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_quota_client.h"

#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace content {

CacheStorageQuotaClient::CacheStorageQuotaClient(
    scoped_refptr<CacheStorageManager> cache_manager,
    storage::mojom::CacheStorageOwner owner)
    : cache_manager_(std::move(cache_manager)), owner_(owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CacheStorageQuotaClient::~CacheStorageQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CacheStorageQuotaClient::GetBucketUsage(
    const storage::BucketLocator& bucket,
    GetBucketUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(bucket.type, blink::mojom::StorageType::kTemporary);

  if (!CacheStorageManager::IsValidQuotaStorageKey(bucket.storage_key)) {
    std::move(callback).Run(0);
    return;
  }

  cache_manager_->GetBucketUsage(bucket, owner_, std::move(callback));
}

void CacheStorageQuotaClient::GetStorageKeysForType(
    blink::mojom::StorageType type,
    GetStorageKeysForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  cache_manager_->GetStorageKeys(owner_, std::move(callback));
}

void CacheStorageQuotaClient::DeleteBucketData(
    const storage::BucketLocator& bucket,
    DeleteBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(bucket.type, blink::mojom::StorageType::kTemporary);

  if (!CacheStorageManager::IsValidQuotaStorageKey(bucket.storage_key)) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  cache_manager_->DeleteBucketData(bucket, owner_, std::move(callback));
}

void CacheStorageQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
}

// static
storage::QuotaClientType CacheStorageQuotaClient::GetClientTypeFromOwner(
    storage::mojom::CacheStorageOwner owner) {
  switch (owner) {
    case storage::mojom::CacheStorageOwner::kCacheAPI:
      return storage::QuotaClientType::kServiceWorkerCache;
    case storage::mojom::CacheStorageOwner::kBackgroundFetch:
      return storage::QuotaClientType::kBackgroundFetch;
  }
}

}  // namespace content

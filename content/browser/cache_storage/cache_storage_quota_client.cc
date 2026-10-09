// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_quota_client.h"

#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "storage/browser/quota/quota_client_type.h"

namespace content {

CacheStorageQuotaClient::CacheStorageQuotaClient(
    base::WeakPtr<CacheStorageManager> cache_manager,
    storage::mojom::CacheStorageOwner owner)
    : cache_manager_(std::move(cache_manager)), owner_(owner) {}

CacheStorageQuotaClient::~CacheStorageQuotaClient() = default;

void CacheStorageQuotaClient::GetBucketUsage(
    const storage::BucketLocator& bucket,
    GetBucketUsageCallback callback) {
  if (!cache_manager_ ||
      !CacheStorageManager::IsValidQuotaStorageKey(bucket.storage_key)) {
    std::move(callback).Run(0);
    return;
  }

  cache_manager_->GetBucketUsage(bucket, owner_, std::move(callback));
}

void CacheStorageQuotaClient::GetDefaultStorageKeys(
    GetDefaultStorageKeysCallback callback) {
  if (cache_manager_) {
    cache_manager_->GetStorageKeys(owner_, std::move(callback));
  } else {
    std::move(callback).Run({});
  }
}

void CacheStorageQuotaClient::DeleteBucketData(
    const storage::BucketLocator& bucket,
    DeleteBucketDataCallback callback) {
  if (!cache_manager_ ||
      !CacheStorageManager::IsValidQuotaStorageKey(bucket.storage_key)) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  cache_manager_->DeleteBucketData(bucket, owner_, std::move(callback));
}

void CacheStorageQuotaClient::PerformStorageCleanup(
    PerformStorageCleanupCallback callback) {
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

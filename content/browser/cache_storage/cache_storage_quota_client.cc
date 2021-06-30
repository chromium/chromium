// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_quota_client.h"

#include "content/browser/cache_storage/cache_storage_manager.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
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

void CacheStorageQuotaClient::GetStorageKeyUsage(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    GetStorageKeyUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  if (!CacheStorageManager::IsValidQuotaStorageKey(storage_key)) {
    std::move(callback).Run(0);
    return;
  }

  cache_manager_->GetStorageKeyUsage(storage_key, owner_, std::move(callback));
}

void CacheStorageQuotaClient::GetStorageKeysForType(
    blink::mojom::StorageType type,
    GetStorageKeysForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  cache_manager_->GetStorageKeys(owner_, std::move(callback));
}

void CacheStorageQuotaClient::GetStorageKeysForHost(
    blink::mojom::StorageType type,
    const std::string& host,
    GetStorageKeysForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  cache_manager_->GetStorageKeysForHost(host, owner_, std::move(callback));
}

void CacheStorageQuotaClient::DeleteStorageKeyData(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    DeleteStorageKeyDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  if (!CacheStorageManager::IsValidQuotaStorageKey(storage_key)) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  cache_manager_->DeleteStorageKeyData(storage_key, owner_,
                                       std::move(callback));
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

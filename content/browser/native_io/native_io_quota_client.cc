// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_quota_client.h"

#include "base/sequence_checker.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/native_io/native_io_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

NativeIOQuotaClient::NativeIOQuotaClient(NativeIOManager* manager)
    : manager_(manager) {}

NativeIOQuotaClient::~NativeIOQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NativeIOQuotaClient::GetBucketUsage(const storage::BucketLocator& bucket,
                                         GetBucketUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(bucket.type, blink::mojom::StorageType::kTemporary);

  // Skip non-default buckets because Storage Buckets are not planned to be
  // supported by NativeIO.
  if (!bucket.is_default) {
    std::move(callback).Run(0);
    return;
  }

  manager_->GetStorageKeyUsage(bucket.storage_key, bucket.type,
                               std::move(callback));
  return;
}

void NativeIOQuotaClient::GetStorageKeysForType(
    blink::mojom::StorageType type,
    GetStorageKeysForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);

  manager_->GetStorageKeysForType(type, std::move(callback));
}

void NativeIOQuotaClient::DeleteBucketData(const storage::BucketLocator& bucket,
                                           DeleteBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(bucket.type, blink::mojom::StorageType::kTemporary);

  // Skip non-default buckets because Storage Buckets are not planned to be
  // supported by NativeIO.
  if (!bucket.is_default) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  manager_->DeleteStorageKeyData(bucket.storage_key, std::move(callback));
}

void NativeIOQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(callback).Run();
}

}  // namespace content

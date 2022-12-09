// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_quota_client.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "storage/browser/database/database_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;
using ::storage::DatabaseUtil;
using ::storage::mojom::QuotaClient;

namespace content {

IndexedDBQuotaClient::IndexedDBQuotaClient(
    IndexedDBContextImpl& indexed_db_context)
    : indexed_db_context_(indexed_db_context) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IndexedDBQuotaClient::~IndexedDBQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBQuotaClient::GetBucketUsage(const storage::BucketLocator& bucket,
                                          GetBucketUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(bucket.type, StorageType::kTemporary);

  std::move(callback).Run(indexed_db_context_->GetBucketDiskUsage(bucket));
}

void IndexedDBQuotaClient::GetStorageKeysForType(
    StorageType type,
    GetStorageKeysForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  const auto& bucket_locators = indexed_db_context_->GetAllBuckets();
  std::vector<StorageKey> storage_keys;
  for (const auto& bucket_locator : bucket_locators)
    storage_keys.push_back(bucket_locator.storage_key);
  std::move(callback).Run(std::move(storage_keys));
}

void IndexedDBQuotaClient::DeleteBucketData(
    const storage::BucketLocator& bucket,
    DeleteBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(bucket.type, StorageType::kTemporary);
  DCHECK(!callback.is_null());

  indexed_db_context_->DeleteBucketData(
      bucket,
      base::BindOnce(
          [](DeleteBucketDataCallback callback, bool success) {
            blink::mojom::QuotaStatusCode status =
                success ? blink::mojom::QuotaStatusCode::kOk
                        : blink::mojom::QuotaStatusCode::kUnknown;
            std::move(callback).Run(status);
          },
          std::move(callback)));
}

void IndexedDBQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  std::move(callback).Run();
}

}  // namespace content

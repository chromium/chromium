// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_quota_client.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_usage_info.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using ::blink::mojom::StorageType;
using ::storage::mojom::QuotaClient;

namespace content {
namespace {
void ReportToQuotaStatus(QuotaClient::DeleteBucketDataCallback callback,
                         blink::ServiceWorkerStatusCode status) {
  std::move(callback).Run((status == blink::ServiceWorkerStatusCode::kOk)
                              ? blink::mojom::QuotaStatusCode::kOk
                              : blink::mojom::QuotaStatusCode::kUnknown);
}

void FindUsageForStorageKey(QuotaClient::GetBucketUsageCallback callback,
                            blink::ServiceWorkerStatusCode status,
                            int64_t usage) {
  std::move(callback).Run(usage);
}
}  // namespace

ServiceWorkerQuotaClient::ServiceWorkerQuotaClient(
    ServiceWorkerContextCore& context)
    : context_(&context) {}

ServiceWorkerQuotaClient::~ServiceWorkerQuotaClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ServiceWorkerQuotaClient::GetBucketUsage(
    const storage::BucketLocator& bucket,
    GetBucketUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(bucket.type, StorageType::kTemporary);

  // Skip non-default buckets until Storage Buckets are supported for
  // ServiceWorkers.
  // TODO(crbug.com/40213545): Integrate ServiceWorkers with StorageBuckets.
  if (!bucket.is_default) {
    std::move(callback).Run(0);
    return;
  }
  context_->registry()->GetStorageUsageForStorageKey(
      bucket.storage_key,
      base::BindOnce(&FindUsageForStorageKey, std::move(callback)));
}

void ServiceWorkerQuotaClient::GetStorageKeysForType(
    StorageType type,
    GetStorageKeysForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  context_->registry()->GetRegisteredStorageKeys(std::move(callback));
}

void ServiceWorkerQuotaClient::DeleteBucketData(
    const storage::BucketLocator& bucket,
    DeleteBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(bucket.type, StorageType::kTemporary);

  // Skip non-default buckets until Storage Buckets are supported for
  // ServiceWorkers.
  // TODO(crbug.com/40213545): Integrate ServiceWorkers with StorageBuckets.
  if (!bucket.is_default) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }
  context_->DeleteForStorageKey(
      bucket.storage_key,
      base::BindOnce(&ReportToQuotaStatus, std::move(callback)));
}

void ServiceWorkerQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, StorageType::kTemporary);
  context_->registry()->PerformStorageCleanup(std::move(callback));
}

}  // namespace content

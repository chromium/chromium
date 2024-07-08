// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_client.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace storage {

MockQuotaClient::MockQuotaClient(
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    QuotaClientType client_type,
    base::span<const UnmigratedStorageKeyData> mock_data)
    : quota_manager_proxy_(std::move(quota_manager_proxy)),
      client_type_(client_type) {
  for (auto& mock_storage_key_data : mock_data) {
    unmigrated_storage_key_data_[{blink::StorageKey::CreateFromStringForTesting(
                                      mock_storage_key_data.origin),
                                  mock_storage_key_data.type}] =
        mock_storage_key_data.usage;
  }
}

MockQuotaClient::~MockQuotaClient() = default;

void MockQuotaClient::AddBucketsData(
    const std::map<BucketLocator, int64_t>& mock_data) {
  bucket_data_.insert(mock_data.begin(), mock_data.end());
}

void MockQuotaClient::ModifyBucketAndNotify(const BucketLocator& bucket,
                                            int64_t delta) {
  auto it = bucket_data_.find(bucket);
  CHECK(it != bucket_data_.end(), base::NotFatalUntil::M130);
  it->second += delta;
  DCHECK_GE(it->second, 0);
  quota_manager_proxy_->NotifyBucketModified(
      client_type_, bucket, delta, IncrementMockTime(),
      base::SequencedTaskRunner::GetCurrentDefault(), base::DoNothing());
}

void MockQuotaClient::AddBucketToErrorSet(const BucketLocator& bucket) {
  error_buckets_.emplace(bucket);
}

base::Time MockQuotaClient::IncrementMockTime() {
  ++mock_time_counter_;
  return base::Time::FromSecondsSinceUnixEpoch(mock_time_counter_ * 10.0);
}

void MockQuotaClient::GetBucketUsage(const BucketLocator& bucket,
                                     GetBucketUsageCallback callback) {
  ++get_bucket_usage_call_count_;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockQuotaClient::RunGetBucketUsage,
                     weak_factory_.GetWeakPtr(), bucket, std::move(callback)));
}

void MockQuotaClient::GetStorageKeysForType(
    blink::mojom::StorageType type,
    GetStorageKeysForTypeCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockQuotaClient::RunGetStorageKeysForType,
                     weak_factory_.GetWeakPtr(), type, std::move(callback)));
}

void MockQuotaClient::DeleteBucketData(const BucketLocator& bucket,
                                       DeleteBucketDataCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockQuotaClient::RunDeleteBucketData,
                     weak_factory_.GetWeakPtr(), bucket, std::move(callback)));
}

void MockQuotaClient::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  std::move(callback).Run();
}

void MockQuotaClient::RunGetBucketUsage(const BucketLocator& bucket,
                                        GetBucketUsageCallback callback) {
  auto it = bucket_data_.find(bucket);
  if (it == bucket_data_.end()) {
    std::move(callback).Run(0);
  } else {
    std::move(callback).Run(it->second);
  }
}

void MockQuotaClient::RunGetStorageKeysForType(
    blink::mojom::StorageType type,
    GetStorageKeysForTypeCallback callback) {
  std::vector<blink::StorageKey> storage_keys;
  for (const auto& storage_key_type_usage : unmigrated_storage_key_data_) {
    if (type == storage_key_type_usage.first.second)
      storage_keys.push_back(storage_key_type_usage.first.first);
  }
  std::move(callback).Run(std::move(storage_keys));
}

void MockQuotaClient::RunDeleteBucketData(const BucketLocator& bucket,
                                          DeleteBucketDataCallback callback) {
  auto error_it = error_buckets_.find(bucket);
  if (error_it != error_buckets_.end()) {
    std::move(callback).Run(
        blink::mojom::QuotaStatusCode::kErrorInvalidModification);
    return;
  }

  auto it = bucket_data_.find(bucket);
  if (it == bucket_data_.end()) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  int64_t delta = it->second;
  quota_manager_proxy_->NotifyBucketModified(
      client_type_, bucket, -delta, base::Time::Now(),
      base::SequencedTaskRunner::GetCurrentDefault(), base::DoNothing());
  bucket_data_.erase(it);
  std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
}

}  // namespace storage

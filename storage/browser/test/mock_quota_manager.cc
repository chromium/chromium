// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_manager.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "storage/browser/quota/quota_client_type.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {

MockQuotaManager::BucketData::BucketData(const BucketInfo& bucket,
                                         QuotaClientTypes quota_client_types,
                                         base::Time modified)
    : bucket(bucket),
      quota_client_types(std::move(quota_client_types)),
      modified(modified) {}

MockQuotaManager::BucketData::~BucketData() = default;

MockQuotaManager::BucketData::BucketData(MockQuotaManager::BucketData&&) =
    default;
MockQuotaManager::BucketData& MockQuotaManager::BucketData::operator=(
    MockQuotaManager::BucketData&&) = default;

MockQuotaManager::StorageInfo::StorageInfo()
    : usage(0), quota(std::numeric_limits<int64_t>::max()) {}
MockQuotaManager::StorageInfo::~StorageInfo() = default;

MockQuotaManager::MockQuotaManager(
    bool is_incognito,
    const base::FilePath& profile_path,
    scoped_refptr<base::SingleThreadTaskRunner> io_thread,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy)
    : QuotaManager(is_incognito,
                   profile_path,
                   std::move(io_thread),
                   /*quota_change_callback=*/base::DoNothing(),
                   std::move(special_storage_policy),
                   GetQuotaSettingsFunc()),
      profile_path_(profile_path) {
  QuotaManagerImpl::SetEvictionDisabledForTesting(false);
}

void MockQuotaManager::UpdateOrCreateBucket(
    const BucketInitParams& params,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  if (db_disabled_) {
    std::move(callback).Run(QuotaError::kDatabaseError);
    return;
  }

  QuotaErrorOr<BucketInfo> bucket_or =
      FindAndUpdateBucket(params, blink::mojom::StorageType::kTemporary);
  if (bucket_or.ok()) {
    std::move(callback).Run(std::move(bucket_or));
    return;
  }
  BucketInfo bucket =
      CreateBucket(params, blink::mojom::StorageType::kTemporary);
  buckets_.emplace_back(
      BucketData(bucket, storage::AllQuotaClientTypes(), base::Time::Now()));
  std::move(callback).Run(std::move(bucket));
}

void MockQuotaManager::CreateBucketForTesting(
    const blink::StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType storage_type,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  const BucketInitParams params = BucketInitParams(storage_key, bucket_name);
  BucketInfo bucket = CreateBucket(params, storage_type);
  buckets_.emplace_back(
      BucketData(bucket, storage::AllQuotaClientTypes(), base::Time::Now()));
  std::move(callback).Run(std::move(bucket));
}

void MockQuotaManager::GetOrCreateBucketDeprecated(
    const BucketInitParams& params,
    blink::mojom::StorageType type,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  if (db_disabled_) {
    std::move(callback).Run(QuotaError::kDatabaseError);
    return;
  }

  QuotaErrorOr<BucketInfo> bucket_or = FindAndUpdateBucket(params, type);
  if (bucket_or.ok()) {
    std::move(callback).Run(std::move(bucket_or));
    return;
  }
  BucketInfo bucket = CreateBucket(params, type);
  buckets_.emplace_back(
      BucketData(bucket, storage::AllQuotaClientTypes(), base::Time::Now()));
  std::move(callback).Run(std::move(bucket));
}

void MockQuotaManager::GetBucketById(
    const BucketId& bucket_id,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  QuotaErrorOr<BucketInfo> bucket = FindBucketById(bucket_id);
  std::move(callback).Run(std::move(bucket));
}

void MockQuotaManager::GetBucket(
    const blink::StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType type,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  QuotaErrorOr<BucketInfo> bucket = FindBucket(storage_key, bucket_name, type);
  std::move(callback).Run(std::move(bucket));
}

void MockQuotaManager::GetUsageAndQuota(const StorageKey& storage_key,
                                        StorageType type,
                                        UsageAndQuotaCallback callback) {
  StorageInfo& info = usage_and_quota_map_[std::make_pair(storage_key, type)];
  std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, info.usage,
                          info.quota);
}

void MockQuotaManager::SetQuota(const StorageKey& storage_key,
                                StorageType type,
                                int64_t quota) {
  usage_and_quota_map_[std::make_pair(storage_key, type)].quota = quota;
}

bool MockQuotaManager::AddBucket(const BucketInfo& bucket,
                                 QuotaClientTypes quota_client_types,
                                 base::Time modified) {
  auto it = std::find_if(
      buckets_.begin(), buckets_.end(),
      [bucket](const BucketData& bucket_data) {
        return bucket.id == bucket_data.bucket.id ||
               (bucket.name == bucket_data.bucket.name &&
                bucket.storage_key == bucket_data.bucket.storage_key &&
                bucket.type == bucket_data.bucket.type);
      });
  DCHECK(it == buckets_.end());
  buckets_.emplace_back(
      BucketData(bucket, std::move(quota_client_types), modified));
  return true;
}

BucketInfo MockQuotaManager::CreateBucket(const BucketInitParams& params,
                                          StorageType type) {
  return BucketInfo(
      bucket_id_generator_.GenerateNextId(), params.storage_key, type,
      params.name, params.expiration, params.quota,
      params.persistent.value_or(false),
      params.durability.value_or(blink::mojom::BucketDurability::kRelaxed));
}

bool MockQuotaManager::BucketHasData(const BucketInfo& bucket,
                                     QuotaClientType quota_client) const {
  for (const auto& info : buckets_) {
    if (info.bucket == bucket && info.quota_client_types.contains(quota_client))
      return true;
  }
  return false;
}

int MockQuotaManager::BucketDataCount(QuotaClientType quota_client) {
  return std::count_if(
      buckets_.begin(), buckets_.end(),
      [quota_client](const BucketData& bucket) {
        return bucket.quota_client_types.contains(quota_client);
      });
}

void MockQuotaManager::GetBucketsModifiedBetween(StorageType type,
                                                 base::Time begin,
                                                 base::Time end,
                                                 GetBucketsCallback callback) {
  auto buckets_to_return = std::make_unique<std::set<BucketLocator>>();
  for (const auto& info : buckets_) {
    if (info.bucket.type == type && info.modified >= begin &&
        info.modified < end)
      buckets_to_return->insert(BucketLocator(
          info.bucket.id, info.bucket.storage_key, info.bucket.type,
          info.bucket.name == kDefaultBucketName));
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaManager::DidGetModifiedInTimeRange,
                                weak_factory_.GetWeakPtr(), std::move(callback),
                                std::move(buckets_to_return), type));
}

void MockQuotaManager::DeleteBucketData(const BucketLocator& bucket,
                                        QuotaClientTypes quota_client_types,
                                        StatusCallback callback) {
  for (auto current = buckets_.begin(); current != buckets_.end(); ++current) {
    if (current->bucket.id == bucket.id) {
      // Modify the mask: if it's 0 after "deletion", remove the storage key.
      for (QuotaClientType type : quota_client_types)
        current->quota_client_types.erase(type);
      if (current->quota_client_types.empty())
        buckets_.erase(current);
      break;
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaManager::DidDeleteBucketData,
                                weak_factory_.GetWeakPtr(), std::move(callback),
                                blink::mojom::QuotaStatusCode::kOk));
}

void MockQuotaManager::FindAndDeleteBucketData(const StorageKey& storage_key,
                                               const std::string& bucket_name,
                                               StatusCallback callback) {
  QuotaErrorOr<BucketInfo> result = FindBucket(
      storage_key, bucket_name, blink::mojom::StorageType::kTemporary);
  if (!result.ok()) {
    if (result.error() == QuotaError::kNotFound) {
      std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    } else {
      std::move(callback).Run(blink::mojom::QuotaStatusCode::kUnknown);
    }
    return;
  }

  DeleteBucketData(result->ToBucketLocator(), AllQuotaClientTypes(),
                   std::move(callback));
}

void MockQuotaManager::UpdateBucketPersistence(
    BucketId bucket,
    bool persistent,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  auto it = std::find_if(buckets_.begin(), buckets_.end(),
                         [bucket](const BucketData& bucket_data) {
                           return bucket_data.bucket.id == bucket;
                         });
  if (it != buckets_.end()) {
    it->bucket.persistent = persistent;
    std::move(callback).Run(it->bucket);
  } else {
    std::move(callback).Run(QuotaError::kNotFound);
  }
}

void MockQuotaManager::NotifyWriteFailed(const StorageKey& storage_key) {
  auto storage_key_error_log =
      write_error_tracker_.insert(std::pair<StorageKey, int>(storage_key, 0))
          .first;
  ++storage_key_error_log->second;
}

MockQuotaManager::~MockQuotaManager() = default;

QuotaErrorOr<BucketInfo> MockQuotaManager::FindBucketById(
    const BucketId& bucket_id) {
  auto it = std::find_if(buckets_.begin(), buckets_.end(),
                         [bucket_id](const BucketData& bucket_data) {
                           return bucket_data.bucket.id == bucket_id;
                         });
  if (it != buckets_.end()) {
    return it->bucket;
  }
  return QuotaError::kNotFound;
}

QuotaErrorOr<BucketInfo> MockQuotaManager::FindBucket(
    const blink::StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType type) {
  auto it = std::find_if(
      buckets_.begin(), buckets_.end(),
      [storage_key, bucket_name, type](const BucketData& bucket_data) {
        return bucket_data.bucket.storage_key == storage_key &&
               bucket_data.bucket.name == bucket_name &&
               bucket_data.bucket.type == type;
      });
  if (it != buckets_.end()) {
    return it->bucket;
  }
  return QuotaError::kNotFound;
}

QuotaErrorOr<BucketInfo> MockQuotaManager::FindAndUpdateBucket(
    const BucketInitParams& params,
    blink::mojom::StorageType type) {
  auto it = std::find_if(buckets_.begin(), buckets_.end(),
                         [params, type](const BucketData& bucket_data) {
                           return bucket_data.bucket.storage_key ==
                                      params.storage_key &&
                                  bucket_data.bucket.name == params.name &&
                                  bucket_data.bucket.type == type;
                         });
  if (it != buckets_.end()) {
    if (params.persistent)
      it->bucket.persistent = *params.persistent;
    if (!params.expiration.is_null())
      it->bucket.expiration = params.expiration;
    return it->bucket;
  }
  return QuotaError::kNotFound;
}

void MockQuotaManager::UpdateUsage(const StorageKey& storage_key,
                                   StorageType type,
                                   int64_t delta) {
  usage_and_quota_map_[std::make_pair(storage_key, type)].usage += delta;
}

void MockQuotaManager::DidGetBucket(
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
    QuotaErrorOr<BucketInfo> result) {
  std::move(callback).Run(std::move(result));
}

void MockQuotaManager::DidGetModifiedInTimeRange(
    GetBucketsCallback callback,
    std::unique_ptr<std::set<BucketLocator>> buckets,
    StorageType storage_type) {
  std::move(callback).Run(*buckets, storage_type);
}

void MockQuotaManager::DidDeleteBucketData(
    StatusCallback callback,
    blink::mojom::QuotaStatusCode status) {
  std::move(callback).Run(status);
}

}  // namespace storage

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_manager.h"

#include <limits>
#include <memory>
#include <tuple>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_waitable_event.h"
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
  // Make sure serialization doesn't fail.
  params.storage_key.Serialize();

  if (db_disabled_) {
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseError));
    return;
  }

  const auto create = [&](auto) -> QuotaErrorOr<BucketInfo> {
    BucketInfo bucket =
        CreateBucket(params, blink::mojom::StorageType::kTemporary);
    buckets_.emplace_back(bucket, storage::AllQuotaClientTypes(),
                          base::Time::Now());
    return bucket;
  };
  std::move(callback).Run(
      FindAndUpdateBucket(params, blink::mojom::StorageType::kTemporary)
          .or_else(create));
}

QuotaErrorOr<BucketInfo> MockQuotaManager::GetOrCreateBucketSync(
    const BucketInitParams& params) {
  QuotaErrorOr<BucketInfo> bucket;
  base::TestWaitableEvent waiter(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  UpdateOrCreateBucket(params, base::BindOnce(
                                   [](base::TestWaitableEvent* waiter,
                                      QuotaErrorOr<BucketInfo>* sync_bucket,
                                      QuotaErrorOr<BucketInfo> result_bucket) {
                                     *sync_bucket = std::move(result_bucket);
                                     waiter->Signal();
                                   },
                                   &waiter, &bucket));
  waiter.Wait();
  return bucket;
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
    std::move(callback).Run(base::unexpected(QuotaError::kDatabaseError));
    return;
  }

  const auto create = [&](auto) -> QuotaErrorOr<BucketInfo> {
    BucketInfo bucket = CreateBucket(params, type);
    buckets_.emplace_back(bucket, storage::AllQuotaClientTypes(),
                          base::Time::Now());
    return bucket;
  };
  std::move(callback).Run(FindAndUpdateBucket(params, type).or_else(create));
}

void MockQuotaManager::GetBucketById(
    const BucketId& bucket_id,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK(bucket_id);
  QuotaErrorOr<BucketInfo> bucket = FindBucketById(bucket_id);
  std::move(callback).Run(std::move(bucket));
}

void MockQuotaManager::GetBucketByNameUnsafe(
    const blink::StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType type,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  QuotaErrorOr<BucketInfo> bucket = FindBucket(storage_key, bucket_name, type);
  std::move(callback).Run(std::move(bucket));
}

void MockQuotaManager::GetBucketsForStorageKey(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback,
    bool delete_expired) {
  // This parameter is not supported.
  DCHECK(!delete_expired);

  std::set<BucketInfo> retval;
  for (const auto& it : buckets_) {
    if (it.bucket.storage_key == storage_key) {
      retval.insert(it.bucket);
    }
  }
  std::move(callback).Run(retval);
}

void MockQuotaManager::GetUsageAndQuota(const StorageKey& storage_key,
                                        StorageType type,
                                        UsageAndQuotaCallback callback) {
  int64_t quota = quota_map_[std::make_pair(storage_key, type)].quota;
  int64_t usage = 0;

  if (usage_map_.empty()) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, usage, quota);
    return;
  }

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      usage_map_.size(), base::BindLambdaForTesting([&]() {
        std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, usage,
                                quota);
      }));
  for (const auto& entry : usage_map_) {
    std::ignore = FindBucket(entry.first).transform([&](BucketInfo result) {
      storage::BucketLocator bucket_locator = result.ToBucketLocator();
      if (bucket_locator.storage_key == storage_key &&
          bucket_locator.type == type) {
        usage += usage_map_[bucket_locator].usage;
      }
    });
    barrier_closure.Run();
  }
}

int64_t MockQuotaManager::GetQuotaForStorageKey(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    const QuotaSettings& settings) const {
  auto quota = quota_map_.find(std::make_pair(storage_key, type));
  if (quota != quota_map_.end()) {
    return quota->second.quota;
  }

  return QuotaManager::GetQuotaForStorageKey(storage_key, type, settings);
}

void MockQuotaManager::SetQuota(const StorageKey& storage_key,
                                StorageType type,
                                int64_t quota) {
  quota_map_[std::make_pair(storage_key, type)].quota = quota;
}

bool MockQuotaManager::AddBucket(const BucketInfo& bucket,
                                 QuotaClientTypes quota_client_types,
                                 base::Time modified) {
  DCHECK(
      base::ranges::none_of(buckets_, [bucket](const BucketData& bucket_data) {
        return bucket.id == bucket_data.bucket.id ||
               (bucket.name == bucket_data.bucket.name &&
                bucket.storage_key == bucket_data.bucket.storage_key &&
                bucket.type == bucket_data.bucket.type);
      }));
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
    if (info.bucket == bucket &&
        info.quota_client_types.contains(quota_client)) {
      return true;
    }
  }
  return false;
}

int MockQuotaManager::BucketDataCount(QuotaClientType quota_client) {
  return base::ranges::count_if(
      buckets_, [quota_client](const BucketData& bucket) {
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
        info.modified < end) {
      buckets_to_return->insert(BucketLocator(
          info.bucket.id, info.bucket.storage_key, info.bucket.type,
          info.bucket.name == kDefaultBucketName));
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaManager::DidGetModifiedInTimeRange,
                                weak_factory_.GetWeakPtr(), std::move(callback),
                                std::move(buckets_to_return)));
}

void MockQuotaManager::DeleteBucketData(const BucketLocator& bucket,
                                        QuotaClientTypes quota_client_types,
                                        StatusCallback callback) {
  for (auto current = buckets_.begin(); current != buckets_.end(); ++current) {
    if (current->bucket.id == bucket.id) {
      // Modify the mask: if it's 0 after "deletion", remove the storage key.
      for (QuotaClientType type : quota_client_types) {
        current->quota_client_types.erase(type);
      }
      if (current->quota_client_types.empty()) {
        buckets_.erase(current);
      }
      break;
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&MockQuotaManager::DidDeleteBucketData,
                                weak_factory_.GetWeakPtr(), std::move(callback),
                                blink::mojom::QuotaStatusCode::kOk));
}

void MockQuotaManager::FindAndDeleteBucketData(const StorageKey& storage_key,
                                               const std::string& bucket_name,
                                               StatusCallback callback) {
  QuotaErrorOr<BucketInfo> result = FindBucket(
      storage_key, bucket_name, blink::mojom::StorageType::kTemporary);
  if (!result.has_value()) {
    std::move(callback).Run((result.error() == QuotaError::kNotFound)
                                ? blink::mojom::QuotaStatusCode::kOk
                                : blink::mojom::QuotaStatusCode::kUnknown);
    return;
  }

  DeleteBucketData(result->ToBucketLocator(), AllQuotaClientTypes(),
                   std::move(callback));
}

void MockQuotaManager::UpdateBucketPersistence(
    BucketId bucket,
    bool persistent,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  auto it = base::ranges::find(
      buckets_, bucket,
      [](const BucketData& bucket_data) { return bucket_data.bucket.id; });
  if (it != buckets_.end()) {
    it->bucket.persistent = persistent;
    std::move(callback).Run(it->bucket);
  } else {
    std::move(callback).Run(base::unexpected(QuotaError::kNotFound));
  }
}

void MockQuotaManager::OnClientWriteFailed(const StorageKey& storage_key) {
  auto storage_key_error_log =
      write_error_tracker_.insert(std::pair<StorageKey, int>(storage_key, 0))
          .first;
  ++storage_key_error_log->second;
}

MockQuotaManager::~MockQuotaManager() = default;

QuotaErrorOr<BucketInfo> MockQuotaManager::FindBucketById(
    const BucketId& bucket_id) {
  auto it = base::ranges::find(
      buckets_, bucket_id,
      [](const BucketData& bucket_data) { return bucket_data.bucket.id; });
  if (it != buckets_.end()) {
    return it->bucket;
  }
  return base::unexpected(QuotaError::kNotFound);
}

QuotaErrorOr<BucketInfo> MockQuotaManager::FindBucket(
    const blink::StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType type) {
  auto it = base::ranges::find_if(buckets_, [storage_key, bucket_name, type](
                                                const BucketData& bucket_data) {
    return bucket_data.bucket.storage_key == storage_key &&
           bucket_data.bucket.name == bucket_name &&
           bucket_data.bucket.type == type;
  });
  if (it != buckets_.end()) {
    return it->bucket;
  }
  return base::unexpected(QuotaError::kNotFound);
}

QuotaErrorOr<BucketInfo> MockQuotaManager::FindBucket(
    const BucketLocator& locator) {
  auto it =
      base::ranges::find_if(buckets_, [locator](const BucketData& bucket_data) {
        return bucket_data.bucket.ToBucketLocator().IsEquivalentTo(locator);
      });
  if (it != buckets_.end()) {
    return it->bucket;
  }
  return base::unexpected(QuotaError::kNotFound);
}

QuotaErrorOr<BucketInfo> MockQuotaManager::FindAndUpdateBucket(
    const BucketInitParams& params,
    blink::mojom::StorageType type) {
  auto it = base::ranges::find_if(
      buckets_, [params, type](const BucketData& bucket_data) {
        return bucket_data.bucket.storage_key == params.storage_key &&
               bucket_data.bucket.name == params.name &&
               bucket_data.bucket.type == type;
      });
  if (it != buckets_.end()) {
    if (params.persistent) {
      it->bucket.persistent = *params.persistent;
    }
    if (!params.expiration.is_null()) {
      it->bucket.expiration = params.expiration;
    }
    return it->bucket;
  }
  return base::unexpected(QuotaError::kNotFound);
}

void MockQuotaManager::UpdateUsage(const BucketLocator& bucket,
                                   std::optional<int64_t> delta) {
  if (delta) {
    usage_map_[bucket].usage += *delta;
  } else {
    usage_map_[bucket].usage = 0;
  }
}

void MockQuotaManager::DidGetBucket(
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
    QuotaErrorOr<BucketInfo> result) {
  std::move(callback).Run(std::move(result));
}

void MockQuotaManager::DidGetModifiedInTimeRange(
    GetBucketsCallback callback,
    std::unique_ptr<std::set<BucketLocator>> buckets) {
  std::move(callback).Run(*buckets);
}

void MockQuotaManager::DidDeleteBucketData(
    StatusCallback callback,
    blink::mojom::QuotaStatusCode status) {
  std::move(callback).Run(status);
}

}  // namespace storage

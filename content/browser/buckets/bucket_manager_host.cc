// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_manager_host.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "content/browser/buckets/bucket_host.h"
#include "content/browser/buckets/bucket_manager.h"
#include "content/browser/buckets/bucket_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

// These enums are used in metrics. Do not reorder or change their values.
// Append new values to the end.
enum class DurabilityMetric {
  kNoneProvided = 0,
  kRelaxed = 1,
  kStrict = 2,
  kMaxValue = kStrict,
};
enum class PersistenceMetric {
  kNoneProvided = 0,
  kNotPersisted = 1,
  kPersisted = 2,
  kMaxValue = kPersisted,
};

BucketManagerHost::BucketManagerHost(BucketManager* manager,
                                     const blink::StorageKey& storage_key)
    : manager_(manager), storage_key_(storage_key) {
  DCHECK(manager != nullptr);

  // base::Unretained is safe here because this BucketManagerHost owns
  // `receivers_`. So, the unretained BucketManagerHost is guaranteed to
  // outlive |receivers_| and the closure that it uses.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &BucketManagerHost::OnReceiverDisconnect, base::Unretained(this)));
}

BucketManagerHost::~BucketManagerHost() = default;

void BucketManagerHost::BindReceiver(
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
    base::WeakPtr<BucketContext> context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver), std::move(context));
}

void BucketManagerHost::OpenBucket(const std::string& name,
                                   blink::mojom::BucketPoliciesPtr policies,
                                   OpenBucketCallback callback) {
  if (!IsValidBucketName(name)) {
    receivers_.ReportBadMessage("Invalid bucket name");
    return;
  }

  storage::BucketInitParams params(storage_key_, name);
  if (policies) {
    if (policies->expires) {
      params.expiration = *policies->expires;
    }

    if (policies->has_quota) {
      if (policies->quota <= 0) {
        receivers_.ReportBadMessage("Invalid quota");
        return;
      }

      params.quota = policies->quota;
    }

    if (policies->has_durability) {
      params.durability = policies->durability;
    }

    if (policies->has_persisted) {
      // Only grant persistence if permitted.
      if (receivers_.current_context() &&
          receivers_.current_context()->GetPermissionStatus(
              blink::PermissionType::DURABLE_STORAGE) ==
              blink::mojom::PermissionStatus::GRANTED) {
        params.persistent = policies->persisted;
      }
    }

    // Count the number of minutes until expiration. This doesn't use the TIMES
    // histogram variant because that counts in milliseconds and caps the max at
    // ~24 days. Note that negative counts are logged as zero, so all
    // expirations less than one minute in the future (including none specified)
    // will be logged in the underflow bucket. Max duration we care about is 500
    // days.
    base::UmaHistogramCustomCounts(
        "Storage.Buckets.Parameters.Expiration",
        (params.expiration - base::Time::Now()).InMinutes(), 1,
        base::Days(500).InMinutes(), 50);
    // Convert quota to kB before logging.
    base::UmaHistogramCustomCounts("Storage.Buckets.Parameters.QuotaKb",
                                   params.quota / 1024, 1,
                                   /* 20 GB */ 20L * 1024 * 1024, 50);
    base::UmaHistogramEnumeration(
        "Storage.Buckets.Parameters.Durability",
        policies->has_durability
            ? policies->durability == blink::mojom::BucketDurability::kStrict
                  ? DurabilityMetric::kStrict
                  : DurabilityMetric::kRelaxed
            : DurabilityMetric::kNoneProvided);
    base::UmaHistogramEnumeration("Storage.Buckets.Parameters.Persisted",
                                  policies->has_persisted
                                      ? policies->persisted
                                            ? PersistenceMetric::kPersisted
                                            : PersistenceMetric::kNotPersisted
                                      : PersistenceMetric::kNoneProvided);
  }

  GetQuotaManagerProxy()->UpdateOrCreateBucket(
      params, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&BucketManagerHost::DidGetBucket,
                     weak_factory_.GetWeakPtr(), receivers_.current_context(),
                     std::move(callback)));
}

void BucketManagerHost::GetBucketForDevtools(
    const std::string& name,
    mojo::PendingReceiver<blink::mojom::BucketHost> receiver) {
  GetQuotaManagerProxy()->GetBucketByNameUnsafe(
      storage_key_, name, blink::mojom::StorageType::kTemporary,
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&BucketManagerHost::DidGetBucketForDevtools,
                     weak_factory_.GetWeakPtr(), receivers_.current_context(),
                     std::move(receiver)));
}

void BucketManagerHost::Keys(KeysCallback callback) {
  GetQuotaManagerProxy()->GetBucketsForStorageKey(
      storage_key_, blink::mojom::StorageType::kTemporary,
      /*delete_expired=*/true, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&BucketManagerHost::DidGetBuckets,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BucketManagerHost::DeleteBucket(const std::string& name,
                                     DeleteBucketCallback callback) {
  if (!IsValidBucketName(name)) {
    receivers_.ReportBadMessage("Invalid bucket name");
    return;
  }

  GetQuotaManagerProxy()->DeleteBucket(
      storage_key_, name, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&BucketManagerHost::DidDeleteBucket,
                     weak_factory_.GetWeakPtr(), name, std::move(callback)));
}

void BucketManagerHost::RemoveBucketHost(storage::BucketId id) {
  DCHECK(base::Contains(bucket_map_, id));
  bucket_map_.erase(id);
}

StoragePartitionImpl* BucketManagerHost::GetStoragePartition() {
  return manager_->storage_partition();
}

storage::QuotaManagerProxy* BucketManagerHost::GetQuotaManagerProxy() {
  return manager_->storage_partition()->GetQuotaManagerProxy();
}

void BucketManagerHost::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  manager_->OnHostReceiverDisconnect(this, base::PassKey<BucketManagerHost>());
}

void BucketManagerHost::DidGetBucket(
    base::WeakPtr<BucketContext> bucket_context,
    OpenBucketCallback callback,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!bucket_context) {
    std::move(callback).Run(mojo::NullRemote(),
                            blink::mojom::BucketError::kUnknown);
    return;
  }

  if (!result.has_value()) {
    auto error = [](storage::QuotaError code) {
      switch (code) {
        case storage::QuotaError::kQuotaExceeded:
          return blink::mojom::BucketError::kQuotaExceeded;
        case storage::QuotaError::kInvalidExpiration:
          return blink::mojom::BucketError::kInvalidExpiration;
        case storage::QuotaError::kNone:
        case storage::QuotaError::kEntryExistsError:
        case storage::QuotaError::kFileOperationError:
          NOTREACHED();
        case storage::QuotaError::kNotFound:
        case storage::QuotaError::kDatabaseError:
        case storage::QuotaError::kDatabaseDisabled:
        case storage::QuotaError::kUnknownError:
        case storage::QuotaError::kStorageKeyError:
          return blink::mojom::BucketError::kUnknown;
      }
    }(result.error().quota_error);
    std::move(callback).Run(mojo::NullRemote(), error);
    return;
  }

  const auto& bucket = result.value();
  auto it = bucket_map_.find(bucket.id);
  if (it == bucket_map_.end()) {
    it = bucket_map_
             .emplace(bucket.id, std::make_unique<BucketHost>(this, bucket))
             .first;
  }

  auto pending_remote = it->second->CreateStorageBucketBinding(bucket_context);
  std::move(callback).Run(std::move(pending_remote),
                          blink::mojom::BucketError::kUnknown);
}

void BucketManagerHost::DidGetBucketForDevtools(
    base::WeakPtr<BucketContext> bucket_context,
    mojo::PendingReceiver<blink::mojom::BucketHost> receiver,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!bucket_context || !result.has_value()) {
    return;
  }

  const auto& bucket = result.value();
  auto it = bucket_map_.find(bucket.id);
  if (it == bucket_map_.end()) {
    it = bucket_map_
             .emplace(bucket.id, std::make_unique<BucketHost>(this, bucket))
             .first;
  }

  it->second->PassStorageBucketBinding(bucket_context, std::move(receiver));
}

void BucketManagerHost::DidGetBuckets(
    KeysCallback callback,
    storage::QuotaErrorOr<std::set<storage::BucketInfo>> buckets) {
  std::vector<std::string> keys;
  for (const auto& bucket : buckets.value_or(std::set<storage::BucketInfo>())) {
    if (!bucket.is_default()) {
      keys.insert(base::ranges::upper_bound(keys, bucket.name), bucket.name);
    }
  }
  std::move(callback).Run(keys, buckets.has_value());
}

void BucketManagerHost::DidDeleteBucket(const std::string& bucket_name,
                                        DeleteBucketCallback callback,
                                        blink::mojom::QuotaStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(status == blink::mojom::QuotaStatusCode::kOk);
}

}  // namespace content

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/usage_tracker.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "storage/browser/quota/client_usage_tracker.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

struct UsageTracker::AccumulateInfo {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  blink::mojom::UsageBreakdownPtr usage_breakdown =
      blink::mojom::UsageBreakdown::New();
};

UsageTracker::UsageTracker(
    QuotaManagerImpl* quota_manager_impl,
    const base::flat_map<mojom::QuotaClient*, QuotaClientType>& client_types,
    blink::mojom::StorageType type,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy)
    : quota_manager_impl_(quota_manager_impl), type_(type) {
  DCHECK(quota_manager_impl_);

  for (const auto& client_and_type : client_types) {
    mojom::QuotaClient* client = client_and_type.first;
    QuotaClientType client_type = client_and_type.second;
    auto [it, inserted] = client_tracker_map_.insert(std::make_pair(
        client_type, std::make_unique<ClientUsageTracker>(
                         this, client, type, special_storage_policy)));
    CHECK(inserted);
  }
}

UsageTracker::~UsageTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UsageTracker::GetGlobalUsage(UsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  global_usage_callbacks_.emplace_back(std::move(callback));
  if (global_usage_callbacks_.size() > 1)
    return;

  quota_manager_impl_->GetBucketsForType(
      type_, base::BindOnce(&UsageTracker::DidGetBucketsForType,
                            weak_factory_.GetWeakPtr()));
}

void UsageTracker::GetStorageKeyUsageWithBreakdown(
    const blink::StorageKey& storage_key,
    UsageWithBreakdownCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<UsageWithBreakdownCallback>& storage_key_callbacks =
      storage_key_usage_callbacks_[storage_key];
  storage_key_callbacks.emplace_back(std::move(callback));
  if (storage_key_callbacks.size() > 1)
    return;

  quota_manager_impl_->GetBucketsForStorageKey(
      storage_key, type_,
      base::BindOnce(&UsageTracker::DidGetBucketsForStorageKey,
                     weak_factory_.GetWeakPtr(), storage_key));
}

void UsageTracker::GetBucketUsageWithBreakdown(
    const BucketLocator& bucket,
    UsageWithBreakdownCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(bucket.id);
  std::vector<UsageWithBreakdownCallback>& bucket_callbacks =
      bucket_usage_callbacks_[bucket];
  bucket_callbacks.emplace_back(std::move(callback));
  if (bucket_callbacks.size() > 1)
    return;

  auto info = std::make_unique<AccumulateInfo>();
  auto* info_ptr = info.get();
  base::RepeatingClosure barrier = base::BarrierClosure(
      client_tracker_map_.size(),
      base::BindOnce(&UsageTracker::FinallySendBucketUsageWithBreakdown,
                     weak_factory_.GetWeakPtr(), std::move(info), bucket));

  for (const auto& [client_type, client_tracker] : client_tracker_map_) {
    client_tracker->GetBucketsUsage(
        {bucket},
        // base::Unretained usage is safe here because BarrierClosure holds
        // the std::unque_ptr that keeps AccumulateInfo alive, and the
        // BarrierClosure will outlive all the AccumulateClientGlobalUsage
        // closures.
        base::BindOnce(&UsageTracker::AccumulateClientUsageWithBreakdown,
                       weak_factory_.GetWeakPtr(), barrier,
                       base::Unretained(info_ptr), client_type));
  }
}

void UsageTracker::UpdateBucketUsageCache(QuotaClientType client_type,
                                          const BucketLocator& bucket,
                                          std::optional<int64_t> delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetClient(client_type).UpdateBucketUsageCache(bucket, delta);
}

void UsageTracker::DeleteBucketCache(QuotaClientType client_type,
                                     const BucketLocator& bucket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(bucket.type, type_);

  GetClient(client_type).DeleteBucketCache(bucket);
}

int64_t UsageTracker::GetCachedUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t usage = 0;
  for (const auto& [client_type, client_tracker] : client_tracker_map_) {
    usage += client_tracker->GetCachedUsage();
  }
  return usage;
}

std::map<std::string, int64_t> UsageTracker::GetCachedHostsUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<std::string, int64_t> host_usage;
  for (const auto& [client_type, client_tracker] : client_tracker_map_) {
    const ClientUsageTracker::BucketUsageMap& usage_map =
        client_tracker->GetCachedBucketsUsage();
    for (const auto& [bucket, usage] : usage_map) {
      host_usage[bucket.storage_key.origin().host()] += usage;
    }
  }
  return host_usage;
}

std::map<blink::StorageKey, int64_t> UsageTracker::GetCachedStorageKeysUsage()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<blink::StorageKey, int64_t> storage_key_usage;
  for (const auto& [client_type, client_tracker] : client_tracker_map_) {
    const ClientUsageTracker::BucketUsageMap& usage_map =
        client_tracker->GetCachedBucketsUsage();
    for (const auto& [bucket, usage] : usage_map) {
      storage_key_usage[bucket.storage_key] += usage;
    }
  }
  return storage_key_usage;
}

std::map<BucketLocator, int64_t> UsageTracker::GetCachedBucketsUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<BucketLocator, int64_t> aggregated_usage;
  for (const auto& [client_type, client_tracker] : client_tracker_map_) {
    const ClientUsageTracker::BucketUsageMap& usage_map =
        client_tracker->GetCachedBucketsUsage();
    for (const auto& [bucket, usage] : usage_map) {
      aggregated_usage[bucket] += usage;
    }
  }
  return aggregated_usage;
}

void UsageTracker::SetUsageCacheEnabled(QuotaClientType client_type,
                                        const blink::StorageKey& storage_key,
                                        bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetClient(client_type).SetUsageCacheEnabled(storage_key, enabled);
}

void UsageTracker::DidGetBucketsForType(
    QuotaErrorOr<std::set<BucketInfo>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto info = std::make_unique<AccumulateInfo>();
  if (!result.has_value()) {
    // Return with invalid values on error.
    info->usage = -1;
    info->unlimited_usage = -1;
    FinallySendGlobalUsage(std::move(info));
    return;
  }

  const std::set<BucketInfo>& buckets = result.value();
  if (buckets.empty()) {
    FinallySendGlobalUsage(std::move(info));
    return;
  }

  std::set<BucketLocator> bucket_locators =
      BucketInfosToBucketLocators(buckets);

  auto* info_ptr = info.get();
  base::RepeatingClosure barrier = base::BarrierClosure(
      client_tracker_map_.size(),
      base::BindOnce(&UsageTracker::FinallySendGlobalUsage,
                     weak_factory_.GetWeakPtr(), std::move(info)));

  for (const auto& [client_type, client_tracker] : client_tracker_map_) {
    client_tracker->GetBucketsUsage(
        bucket_locators,
        // base::Unretained usage is safe here because BarrierClosure holds
        // the std::unque_ptr that keeps AccumulateInfo alive, and the
        // BarrierClosure will outlive all the AccumulateClientGlobalUsage
        // closures.
        base::BindOnce(&UsageTracker::AccumulateClientGlobalUsage,
                       weak_factory_.GetWeakPtr(), barrier,
                       base::Unretained(info_ptr)));
  }
}

void UsageTracker::DidGetBucketsForStorageKey(
    const blink::StorageKey& storage_key,
    QuotaErrorOr<std::set<BucketInfo>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto info = std::make_unique<AccumulateInfo>();
  if (!result.has_value()) {
    // Return with invalid values on error.
    info->usage = -1;
    info->unlimited_usage = -1;
    FinallySendStorageKeyUsageWithBreakdown(std::move(info), storage_key);
    return;
  }

  const std::set<BucketInfo>& buckets = result.value();
  if (buckets.empty()) {
    FinallySendStorageKeyUsageWithBreakdown(std::move(info), storage_key);
    return;
  }

  std::set<BucketLocator> bucket_locators =
      BucketInfosToBucketLocators(buckets);

  auto* info_ptr = info.get();
  base::RepeatingClosure barrier = base::BarrierClosure(
      client_tracker_map_.size(),
      base::BindOnce(&UsageTracker::FinallySendStorageKeyUsageWithBreakdown,
                     weak_factory_.GetWeakPtr(), std::move(info), storage_key));

  for (const auto& [client_type, client_tracker] : client_tracker_map_) {
    client_tracker->GetBucketsUsage(
        bucket_locators,
        // base::Unretained usage is safe here because BarrierClosure holds
        // the std::unque_ptr that keeps AccumulateInfo alive, and the
        // BarrierClosure will outlive all the AccumulateClientGlobalUsage
        // closures.
        base::BindOnce(&UsageTracker::AccumulateClientUsageWithBreakdown,
                       weak_factory_.GetWeakPtr(), barrier,
                       base::Unretained(info_ptr), client_type));
  }
}

void UsageTracker::AccumulateClientGlobalUsage(
    base::OnceClosure barrier_callback,
    AccumulateInfo* info,
    int64_t total_usage,
    int64_t unlimited_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(unlimited_usage, 0);
  DCHECK_GE(total_usage, unlimited_usage);

  info->usage += total_usage;
  info->unlimited_usage += unlimited_usage;

  std::move(barrier_callback).Run();
}

void UsageTracker::AccumulateClientUsageWithBreakdown(
    base::OnceClosure barrier_callback,
    AccumulateInfo* info,
    QuotaClientType client,
    int64_t total_usage,
    int64_t unlimited_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(unlimited_usage, 0);
  DCHECK_GE(total_usage, unlimited_usage);

  info->usage += total_usage;

  switch (client) {
    case QuotaClientType::kFileSystem:
      info->usage_breakdown->fileSystem += total_usage;
      break;
    case QuotaClientType::kDatabase:
      info->usage_breakdown->webSql += total_usage;
      break;
    case QuotaClientType::kIndexedDatabase:
      info->usage_breakdown->indexedDatabase += total_usage;
      break;
    case QuotaClientType::kServiceWorkerCache:
      info->usage_breakdown->serviceWorkerCache += total_usage;
      break;
    case QuotaClientType::kServiceWorker:
      info->usage_breakdown->serviceWorker += total_usage;
      break;
    case QuotaClientType::kBackgroundFetch:
      info->usage_breakdown->backgroundFetch += total_usage;
      break;
    case QuotaClientType::kMediaLicense:
      // Media license data does not count against quota and should always
      // report 0 usage.
      // TODO(crbug.com/40218094): Consider counting media license data against
      // quota.
      DCHECK_EQ(total_usage, 0);
      break;
  }

  std::move(barrier_callback).Run();
}

void UsageTracker::FinallySendGlobalUsage(
    std::unique_ptr<AccumulateInfo> info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(info->unlimited_usage, -1);
  DCHECK_GE(info->usage, info->unlimited_usage);

  // Moving callbacks out of the original vector early handles the case where a
  // callback makes a new quota call.
  std::vector<UsageCallback> pending_callbacks;
  pending_callbacks.swap(global_usage_callbacks_);
  for (auto& callback : pending_callbacks)
    std::move(callback).Run(info->usage, info->unlimited_usage);
}

void UsageTracker::FinallySendStorageKeyUsageWithBreakdown(
    std::unique_ptr<AccumulateInfo> info,
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = storage_key_usage_callbacks_.find(storage_key);
  if (it == storage_key_usage_callbacks_.end())
    return;

  std::vector<UsageWithBreakdownCallback> pending_callbacks;
  pending_callbacks.swap(it->second);
  DCHECK(pending_callbacks.size() > 0) << "storage_key_usage_callbacks_ should "
                                          "only have non-empty callback lists";
  storage_key_usage_callbacks_.erase(it);

  for (auto& callback : pending_callbacks)
    std::move(callback).Run(info->usage, info->usage_breakdown->Clone());
}

void UsageTracker::FinallySendBucketUsageWithBreakdown(
    std::unique_ptr<AccumulateInfo> info,
    const BucketLocator& bucket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = bucket_usage_callbacks_.find(bucket);
  if (it == bucket_usage_callbacks_.end())
    return;

  std::vector<UsageWithBreakdownCallback> pending_callbacks;
  pending_callbacks.swap(it->second);
  DCHECK(pending_callbacks.size() > 0)
      << "bucket_usage_callbacks_ should only have non-empty callback lists";
  bucket_usage_callbacks_.erase(it);

  for (auto& callback : pending_callbacks)
    std::move(callback).Run(info->usage, info->usage_breakdown->Clone());
}

ClientUsageTracker& UsageTracker::GetClient(QuotaClientType type) {
  auto iter = client_tracker_map_.find(type);
  CHECK(iter != client_tracker_map_.end());
  return *iter->second;
}

}  // namespace storage

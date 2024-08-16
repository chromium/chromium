// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/client_usage_tracker.h"

#include <stdint.h>
#include <iterator>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

struct ClientUsageTracker::AccumulateInfo {
  int64_t limited_usage = 0;
  int64_t unlimited_usage = 0;
};

ClientUsageTracker::ClientUsageTracker(
    UsageTracker* tracker,
    mojom::QuotaClient* client,
    blink::mojom::StorageType type,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy)
    : client_(client),
      type_(type),
      special_storage_policy_(std::move(special_storage_policy)) {
  DCHECK(client_);
  if (special_storage_policy_.get())
    special_storage_policy_->AddObserver(this);
}

ClientUsageTracker::~ClientUsageTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (special_storage_policy_.get())
    special_storage_policy_->RemoveObserver(this);
}

void ClientUsageTracker::GetBucketsUsage(const std::set<BucketLocator>& buckets,
                                         UsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(buckets.size(), 0u);

  auto info = std::make_unique<AccumulateInfo>();
  auto* info_ptr = info.get();
  base::RepeatingClosure barrier = base::BarrierClosure(
      buckets.size(),
      base::BindOnce(&ClientUsageTracker::FinallySendBucketsUsage,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(info)));

  for (const auto& bucket : buckets) {
    // Use a cached usage value, if we have one.
    int64_t cached_usage = GetCachedBucketUsage(bucket);
    if (cached_usage != -1) {
      AccumulateBucketsUsage(barrier, bucket, info_ptr, cached_usage);
      continue;
    }

    client_->GetBucketUsage(
        bucket,
        // base::Unretained usage is safe here because barrier holds the
        // std::unque_ptr that keeps AccumulateInfo alive, and the barrier
        // will outlive all the AccumulateClientGlobalUsage closures.
        base::BindOnce(&ClientUsageTracker::AccumulateBucketsUsage,
                       weak_factory_.GetWeakPtr(), barrier, bucket,
                       base::Unretained(info_ptr)));
  }
}

void ClientUsageTracker::UpdateBucketUsageCache(const BucketLocator& bucket,
                                                std::optional<int64_t> delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsUsageCacheEnabledForStorageKey(bucket.storage_key)) {
    return;
  }

  auto bucket_it = cached_bucket_usage_.find(bucket);
  if (bucket_it == cached_bucket_usage_.end()) {
    return;
  }

  if (delta.has_value()) {
    // Constrain `delta` to avoid negative usage values.
    // TODO(crbug.com/40408082): At least one storage API sends deltas that
    // result in negative total usage. The line below works around this bug.
    // Fix the bug, and remove the workaround.
    bucket_it->second += std::max(*delta, -bucket_it->second);
  } else {
    cached_bucket_usage_.erase(bucket_it);
  }
}

void ClientUsageTracker::DeleteBucketCache(const BucketLocator& bucket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cached_bucket_usage_.erase(bucket);
}

int64_t ClientUsageTracker::GetCachedUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t usage = 0;
  for (const auto& bucket_and_usage : cached_bucket_usage_)
    usage += bucket_and_usage.second;
  return usage;
}

const ClientUsageTracker::BucketUsageMap&
ClientUsageTracker::GetCachedBucketsUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cached_bucket_usage_;
}

void ClientUsageTracker::SetUsageCacheEnabled(
    const blink::StorageKey& storage_key,
    bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (enabled) {
    non_cached_limited_storage_keys_.erase(storage_key);
    non_cached_unlimited_storage_keys_.erase(storage_key);
    return;
  }

  // Find all buckets for `storage_key` in `cached_bucket_usage_`
  // and remove them from the cache. Erases cached bucket usage while iterating.
  for (auto it = cached_bucket_usage_.cbegin();
       it != cached_bucket_usage_.cend();) {
    if (it->first.storage_key == storage_key) {
      // Calling erase() while iterating is safe because (1) std::map::erase()
      // only invalidates the iterator pointing to the erased element, and (2)
      // `it` is advanced off of the erased element before erase() is called.
      cached_bucket_usage_.erase(it++);
    } else {
      ++it;
    }
  }

  // Add to `non_cached_*_storage_keys_` to exclude `storage_key` from quota
  // restrictions.
  if (IsStorageUnlimited(storage_key)) {
    non_cached_unlimited_storage_keys_.insert(storage_key);
  } else {
    non_cached_limited_storage_keys_.insert(storage_key);
  }
}

bool ClientUsageTracker::IsUsageCacheEnabledForStorageKey(
    const blink::StorageKey& storage_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !base::Contains(non_cached_limited_storage_keys_, storage_key) &&
         !base::Contains(non_cached_unlimited_storage_keys_, storage_key);
}

void ClientUsageTracker::AccumulateBucketsUsage(
    base::OnceClosure barrier_callback,
    const BucketLocator& bucket,
    AccumulateInfo* info,
    int64_t usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Defend against confusing inputs from clients.
  // TODO(crbug.com/40213066): Remove this check after fixing QuotaClients.
  if (usage < 0)
    usage = 0;

  if (IsStorageUnlimited(bucket.storage_key)) {
    info->unlimited_usage += usage;
  } else {
    info->limited_usage += usage;
  }

  if (IsUsageCacheEnabledForStorageKey(bucket.storage_key))
    CacheBucketUsage(bucket, usage);
  std::move(barrier_callback).Run();
}

void ClientUsageTracker::FinallySendBucketsUsage(
    UsageCallback callback,
    std::unique_ptr<AccumulateInfo> info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(info->limited_usage, 0);
  DCHECK_GE(info->unlimited_usage, 0);

  std::move(callback).Run(info->limited_usage + info->unlimited_usage,
                          info->unlimited_usage);
}

void ClientUsageTracker::CacheBucketUsage(const BucketLocator& bucket,
                                          int64_t new_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsUsageCacheEnabledForStorageKey(bucket.storage_key));
  cached_bucket_usage_[bucket] = new_usage;
}

int64_t ClientUsageTracker::GetCachedBucketUsage(
    const BucketLocator& bucket) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto bucket_it = cached_bucket_usage_.find(bucket);
  if (bucket_it == cached_bucket_usage_.end())
    return -1;
  return bucket_it->second;
}

void ClientUsageTracker::GetBucketUsage(const BucketLocator& bucket,
                                        UsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->GetBucketUsage(
      bucket,
      base::BindOnce(&ClientUsageTracker::DidGetBucketUsage,
                     weak_factory_.GetWeakPtr(), bucket, std::move(callback)));
  return;
}

void ClientUsageTracker::DidGetBucketUsage(const BucketLocator& bucket,
                                           UsageCallback callback,
                                           int64_t usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsUsageCacheEnabledForStorageKey(bucket.storage_key))
    CacheBucketUsage(bucket, usage);

  int64_t unlimited_usage = IsStorageUnlimited(bucket.storage_key) ? usage : 0;
  std::move(callback).Run(usage, unlimited_usage);
}

void ClientUsageTracker::OnGranted(const url::Origin& origin_url,
                                   int change_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40184305): Remove this conversion once the storage policy
  // APIs are converted to use StorageKey instead of Origin.
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(origin_url);
  if (change_flags & SpecialStoragePolicy::STORAGE_UNLIMITED) {
    if (non_cached_limited_storage_keys_.erase(storage_key))
      non_cached_unlimited_storage_keys_.insert(storage_key);
  }
}

void ClientUsageTracker::OnRevoked(const url::Origin& origin_url,
                                   int change_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40184305): Remove this conversion once the storage policy
  // APIs are converted to use StorageKey instead of Origin.
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(origin_url);
  if (change_flags & SpecialStoragePolicy::STORAGE_UNLIMITED) {
    if (non_cached_unlimited_storage_keys_.erase(storage_key))
      non_cached_limited_storage_keys_.insert(storage_key);
  }
}

void ClientUsageTracker::OnCleared() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  non_cached_limited_storage_keys_.insert(
      std::make_move_iterator(non_cached_unlimited_storage_keys_.begin()),
      std::make_move_iterator(non_cached_unlimited_storage_keys_.end()));
  non_cached_unlimited_storage_keys_.clear();
}

bool ClientUsageTracker::IsStorageUnlimited(
    const blink::StorageKey& storage_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (type_ == blink::mojom::StorageType::kSyncable)
    return false;
  return special_storage_policy_.get() &&
         special_storage_policy_->IsStorageUnlimited(
             storage_key.origin().GetURL());
}

}  // namespace storage

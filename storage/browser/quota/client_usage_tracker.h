// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_CLIENT_USAGE_TRACKER_H_
#define STORAGE_BROWSER_QUOTA_CLIENT_USAGE_TRACKER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_task.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {

class UsageTracker;

// Holds per-client usage tracking information and caches bucket usage data.
//
// A UsageTracker object will own one ClientUsageTracker instance per client.
// This class is not thread-safe. All methods other than the constructor must be
// called on the same sequence.
class ClientUsageTracker : public SpecialStoragePolicy::Observer {
 public:
  using BucketUsageMap =
      std::map<BucketLocator, int64_t, CompareBucketLocators>;

  // The caller must ensure that `client` outlives this instance.
  ClientUsageTracker(
      UsageTracker* tracker,
      mojom::QuotaClient* client,
      blink::mojom::StorageType type,
      scoped_refptr<SpecialStoragePolicy> special_storage_policy);

  ClientUsageTracker(const ClientUsageTracker&) = delete;
  ClientUsageTracker& operator=(const ClientUsageTracker&) = delete;

  ~ClientUsageTracker() override;

  // Computes total usage and unlimited usage for `buckets`.
  void GetBucketsUsage(const std::set<BucketLocator>& buckets,
                       UsageCallback callback);

  // Reflects an increase by `delta` to `bucket`'s quota usage.
  //
  // This will be ignored if called with a `bucket` whose usage is not yet
  // cached. If `delta` is nullopt, the usage will be removed from the cache and
  // later re-calculated as needed. A negative `delta` value reflects a
  // reduction in quota usage. Negative `delta` values are clamped to ensure the
  // total cached usage never goes below zero (crbug.com/463729).
  void UpdateBucketUsageCache(const BucketLocator& bucket,
                              std::optional<int64_t> delta);

  // Deletes `bucket` from the cache if it exists. Called either for bucket
  // deletion or disabling cache for `bucket`'s Storage Key.
  void DeleteBucketCache(const BucketLocator& bucket);

  // Accumulates all cached usage to determine storage pressure.
  int64_t GetCachedUsage() const;

  // Returns cached usage organized by bucket. Used for histogram recording and
  // eviction. Expected to be called after GetGlobalUsage which retrieves and
  // caches usage.
  const BucketUsageMap& GetCachedBucketsUsage() const;

  // Sets if a `storage_key` for `client_` should / should not be excluded from
  // quota restrictions.
  void SetUsageCacheEnabled(const blink::StorageKey& storage_key, bool enabled);

 private:
  struct AccumulateInfo;

  bool IsUsageCacheEnabledForStorageKey(
      const blink::StorageKey& storage_key) const;

  void AccumulateBucketsUsage(base::OnceClosure barrier_callback,
                              const BucketLocator& bucket,
                              AccumulateInfo* info,
                              int64_t usage);

  void FinallySendBucketsUsage(UsageCallback callback,
                               std::unique_ptr<AccumulateInfo> info);

  // Adds `bucket` and its `usage` to the cache. An existing cached value is
  // replaced with the new value provided here. Used by tasks that gather
  // global/host usage to incrementally cache as usage is retrieved.
  void CacheBucketUsage(const BucketLocator& bucket, int64_t usage);

  // Gets cached `bucket` usage. Returns -1 if no usage is cached.
  int64_t GetCachedBucketUsage(const BucketLocator& bucket) const;

  // Retrieves `bucket` usage from the tracked QuotaClient and adds to the
  // cache.
  void GetBucketUsage(const BucketLocator& bucket, UsageCallback callback);
  void DidGetBucketUsage(const BucketLocator& bucket,
                         UsageCallback callback,
                         int64_t usage);

  // SpecialStoragePolicy::Observer overrides.
  // TODO(crbug.com/40184305): Migrate to use StorageKey when the StoragePolicy
  // is migrated to use StorageKey instead of Origin.
  void OnGranted(const url::Origin& origin_url, int change_flags) override;
  void OnRevoked(const url::Origin& origin_url, int change_flags) override;
  void OnCleared() override;

  bool IsStorageUnlimited(const blink::StorageKey& storage_key) const;

  raw_ptr<mojom::QuotaClient> client_;
  const blink::mojom::StorageType type_;

  // The implementation relies on a collection whose erase() only invalidates
  // iterators that point to the erased element. This comment is intended to
  // prevent accidental conversion to other containers, such as base::flat_map.
  BucketUsageMap cached_bucket_usage_;

  // Storage Keys that are excluded from quota restrictions.
  std::set<blink::StorageKey> non_cached_limited_storage_keys_;
  std::set<blink::StorageKey> non_cached_unlimited_storage_keys_;

  const scoped_refptr<SpecialStoragePolicy> special_storage_policy_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ClientUsageTracker> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_CLIENT_USAGE_TRACKER_H_

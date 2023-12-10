// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_USAGE_TRACKER_H_
#define STORAGE_BROWSER_QUOTA_USAGE_TRACKER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_task.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {

class ClientUsageTracker;

// A helper class that gathers and tracks the amount of data stored in
// all quota clients.
//
// Ownership: Each QuotaManagerImpl instance owns 3 instances of this class (one
// per storage type: Persistent, Temporary, Syncable). Thread-safety: All
// methods except the constructor must be called on the same sequence.
class COMPONENT_EXPORT(STORAGE_BROWSER) UsageTracker
    : public QuotaTaskObserver {
 public:
  // The caller must ensure that all mojo::QuotaClient instances outlive this
  // instance.
  UsageTracker(
      QuotaManagerImpl* quota_manager_impl,
      const base::flat_map<mojom::QuotaClient*, QuotaClientType>& client_types,
      blink::mojom::StorageType type,
      scoped_refptr<SpecialStoragePolicy> special_storage_policy);

  UsageTracker(const UsageTracker&) = delete;
  UsageTracker& operator=(const UsageTracker&) = delete;

  ~UsageTracker() override;

  // Retrieves all buckets for type from QuotaDatabase and requests bucket usage
  // from each registered client. Returns cached bucket usage if one exists for
  // a bucket.
  void GetGlobalUsage(UsageCallback callback);

  // Retrieves all buckets for a `storage_key` from QuotaDatabase and requests
  // bucket usage from each registered client. Returns cached bucket usage if
  // one exists for a bucket.
  void GetStorageKeyUsageWithBreakdown(const blink::StorageKey& storage_key,
                                       UsageWithBreakdownCallback callback);

  // Requests bucket usage from each registered client. Returns cached bucket
  // usage if one exists for a bucket.
  void GetBucketUsageWithBreakdown(const BucketLocator& bucket,
                                   UsageWithBreakdownCallback callback);

  // Updates usage for `bucket` in the ClientUsageTracker for `client_type`.
  void UpdateBucketUsageCache(QuotaClientType client_type,
                              const BucketLocator& bucket,
                              std::optional<int64_t> delta);

  // Deletes `bucket` from the cache for `client_type` if it exists.
  // Called by QuotaManagerImpl::BucketDataDeleter.
  void DeleteBucketCache(QuotaClientType client_type,
                         const BucketLocator& bucket);

  // Returns accumulated usage for all cached buckets from registered
  // ClientUsageTrackers. Used to determine storage pressure.
  int64_t GetCachedUsage() const;

  // Retrieves all cached usage organized by host. Expected to be called after
  // GetGlobalUsage which retrieves and caches host usage.
  std::map<std::string, int64_t> GetCachedHostsUsage() const;

  // Returns all cached usage organized by StorageKey. Used for histogram
  // recording.
  std::map<blink::StorageKey, int64_t> GetCachedStorageKeysUsage() const;

  // Returns all cached usage organized by bucket. Used for eviction.
  std::map<BucketLocator, int64_t> GetCachedBucketsUsage() const;

  // Checks if there are ongoing tasks to get usage. Used to prevent a
  // UsageTracker reset from happening before a task is complete.
  bool IsWorking() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !global_usage_callbacks_.empty() ||
           !storage_key_usage_callbacks_.empty() ||
           !bucket_usage_callbacks_.empty();
  }

  // Sets if a `storage_key` for `client_type` should / should not be excluded
  // from quota restrictions.
  void SetUsageCacheEnabled(QuotaClientType client_type,
                            const blink::StorageKey& storage_key,
                            bool enabled);

 private:
  struct AccumulateInfo;
  friend class ClientUsageTracker;

  void DidGetBucketsForType(QuotaErrorOr<std::set<BucketInfo>> result);
  void DidGetBucketsForStorageKey(const blink::StorageKey& storage_key,
                                  QuotaErrorOr<std::set<BucketInfo>> result);

  void AccumulateClientGlobalUsage(base::OnceClosure barrier_callback,
                                   AccumulateInfo* info,
                                   int64_t total_usage,
                                   int64_t unlimited_usage);
  void AccumulateClientUsageWithBreakdown(base::OnceClosure barrier_callback,
                                          AccumulateInfo* info,
                                          QuotaClientType client,
                                          int64_t total_usage,
                                          int64_t unlimited_usage);

  void FinallySendGlobalUsage(std::unique_ptr<AccumulateInfo> info);
  void FinallySendStorageKeyUsageWithBreakdown(
      std::unique_ptr<AccumulateInfo> info,
      const blink::StorageKey& storage_key);
  void FinallySendBucketUsageWithBreakdown(std::unique_ptr<AccumulateInfo> info,
                                           const BucketLocator& bucket);

  ClientUsageTracker& GetClient(QuotaClientType type);

  SEQUENCE_CHECKER(sequence_checker_);

  // Raw pointer usage is safe because `quota_manager_impl_` owns `this` and
  // is therefore valid throughout its lifetime.
  const raw_ptr<QuotaManagerImpl> quota_manager_impl_;
  const blink::mojom::StorageType type_;
  base::flat_map<QuotaClientType, std::unique_ptr<ClientUsageTracker>>
      client_tracker_map_;

  std::vector<UsageCallback> global_usage_callbacks_;
  std::map<blink::StorageKey, std::vector<UsageWithBreakdownCallback>>
      storage_key_usage_callbacks_;
  std::map<BucketLocator, std::vector<UsageWithBreakdownCallback>>
      bucket_usage_callbacks_;

  base::WeakPtrFactory<UsageTracker> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_USAGE_TRACKER_H_

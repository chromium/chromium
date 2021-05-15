// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_CLIENT_USAGE_TRACKER_H_
#define STORAGE_BROWSER_QUOTA_CLIENT_USAGE_TRACKER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_task.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

class UsageTracker;

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "InvalidOriginReason" in src/tools/metrics/histograms/enums.xml.
enum class InvalidOriginReason {
  kIsOpaque = 0,
  kIsEmpty = 1,
  kMaxValue = kIsEmpty
};

// Holds per-client usage tracking information and caches
// per-host usage data.
//
// A UsageTracker object will own one ClientUsageTracker instance per client.
// This class is not thread-safe. All methods other than the constructor must be
// called on the same sequence.
class ClientUsageTracker : public SpecialStoragePolicy::Observer {
 public:
  using OriginSetByHost = std::map<std::string, std::set<url::Origin>>;

  ClientUsageTracker(
      UsageTracker* tracker,
      scoped_refptr<QuotaClient> client,
      blink::mojom::StorageType type,
      scoped_refptr<SpecialStoragePolicy> special_storage_policy);

  ClientUsageTracker(const ClientUsageTracker&) = delete;
  ClientUsageTracker& operator=(const ClientUsageTracker&) = delete;

  ~ClientUsageTracker() override;

  void GetGlobalUsage(GlobalUsageCallback callback);
  void GetHostUsage(const std::string& host, UsageCallback callback);
  void UpdateUsageCache(const url::Origin& origin, int64_t delta);
  int64_t GetCachedUsage() const;
  std::map<std::string, int64_t> GetCachedHostsUsage() const;
  std::map<url::Origin, int64_t> GetCachedOriginsUsage() const;
  std::set<url::Origin> GetCachedOrigins() const;
  bool IsUsageCacheEnabledForOrigin(const url::Origin& origin) const;
  void SetUsageCacheEnabled(const url::Origin& origin, bool enabled);
 private:
  using UsageMap = std::map<url::Origin, int64_t>;

  struct AccumulateInfo;

  void DidGetOriginsForGlobalUsage(GlobalUsageCallback callback,
                                   const std::vector<url::Origin>& origins);
  void AccumulateHostUsage(AccumulateInfo* info,
                           GlobalUsageCallback* callback,
                           int64_t limited_usage,
                           int64_t unlimited_usage);

  void DidGetOriginsForHostUsage(const std::string& host,
                                 const std::vector<url::Origin>& origins);

  void GetUsageForOrigins(const std::string& host,
                          const std::vector<url::Origin>& origins);
  void AccumulateOriginUsage(AccumulateInfo* info,
                             const std::string& host,
                             const absl::optional<url::Origin>& origin,
                             int64_t usage);

  // Methods used by our GatherUsage tasks, as a task makes progress
  // origins and hosts are added incrementally to the cache.
  void AddCachedOrigin(const url::Origin& origin, int64_t usage);
  void AddCachedHost(const std::string& host);

  int64_t GetCachedHostUsage(const std::string& host) const;
  int64_t GetCachedGlobalUnlimitedUsage();
  bool GetCachedOriginUsage(const url::Origin& origin, int64_t* usage) const;

  // SpecialStoragePolicy::Observer overrides
  void OnGranted(const url::Origin& origin_url, int change_flags) override;
  void OnRevoked(const url::Origin& origin_url, int change_flags) override;
  void OnCleared() override;

  void UpdateGlobalUsageValue(int64_t* usage_value, int64_t delta);

  bool IsStorageUnlimited(const url::Origin& origin) const;

  scoped_refptr<QuotaClient> client_;
  const blink::mojom::StorageType type_;

  int64_t global_limited_usage_;
  int64_t global_unlimited_usage_;
  bool global_usage_retrieved_;
  std::set<std::string> cached_hosts_;
  std::map<std::string, UsageMap> cached_usage_by_host_;

  OriginSetByHost non_cached_limited_origins_by_host_;
  OriginSetByHost non_cached_unlimited_origins_by_host_;

  CallbackQueueMap<
      base::OnceCallback<void(int64_t limited_usage, int64_t unlimited_usage)>,
      std::string,
      int64_t,
      int64_t>
      host_usage_accumulators_;

  const scoped_refptr<SpecialStoragePolicy> special_storage_policy_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ClientUsageTracker> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_CLIENT_USAGE_TRACKER_H_

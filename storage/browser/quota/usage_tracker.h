// Copyright 2013 The Chromium Authors. All rights reserved.
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

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_task.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

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
  // TODO(crbug.com/1163009): Switch the map key type in `client_types` to
  //                          mojom::QuotaClient* after all QuotaClients have
  //                          been mojofied.
  UsageTracker(
      const base::flat_map<QuotaClient*, QuotaClientType>& client_types,
      blink::mojom::StorageType type,
      scoped_refptr<SpecialStoragePolicy> special_storage_policy);

  UsageTracker(const UsageTracker&) = delete;
  UsageTracker& operator=(const UsageTracker&) = delete;

  ~UsageTracker() override;

  blink::mojom::StorageType type() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return type_;
  }

  void GetGlobalUsage(GlobalUsageCallback callback);
  void GetHostUsageWithBreakdown(const std::string& host,
                                 UsageWithBreakdownCallback callback);
  void UpdateUsageCache(QuotaClientType client_type,
                        const url::Origin& origin,
                        int64_t delta);
  int64_t GetCachedUsage() const;
  std::map<std::string, int64_t> GetCachedHostsUsage() const;
  std::map<url::Origin, int64_t> GetCachedOriginsUsage() const;
  std::set<url::Origin> GetCachedOrigins() const;
  bool IsWorking() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !global_usage_callbacks_.empty() || !host_usage_callbacks_.empty();
  }

  void SetUsageCacheEnabled(QuotaClientType client_type,
                            const url::Origin& origin,
                            bool enabled);

 private:
  struct AccumulateInfo;
  friend class ClientUsageTracker;

  void AccumulateClientGlobalUsage(AccumulateInfo* info,
                                   int64_t usage,
                                   int64_t unlimited_usage);
  void AccumulateClientHostUsage(base::OnceClosure callback,
                                 AccumulateInfo* info,
                                 const std::string& host,
                                 QuotaClientType client,
                                 int64_t usage);
  void FinallySendHostUsageWithBreakdown(AccumulateInfo* info,
                                         const std::string& host);

  const blink::mojom::StorageType type_;
  base::flat_map<QuotaClientType,
                 std::vector<std::unique_ptr<ClientUsageTracker>>>
      client_tracker_map_;
  size_t client_count_;

  std::vector<GlobalUsageCallback> global_usage_callbacks_;
  std::map<std::string, std::vector<UsageWithBreakdownCallback>>
      host_usage_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<UsageTracker> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_USAGE_TRACKER_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_endpoint_manager.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/time/tick_clock.h"
#include "net/base/backoff_entry.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/rand_callback.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_target_type.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {

class ReportingEndpointManagerImpl : public ReportingEndpointManager {
 public:
  ReportingEndpointManagerImpl(const ReportingPolicy* policy,
                               const base::TickClock* tick_clock,
                               const ReportingDelegate* delegate,
                               ReportingCache* cache,
                               const RandIntCallback& rand_callback)
      : policy_(policy),
        tick_clock_(tick_clock),
        delegate_(delegate),
        cache_(cache),
        rand_callback_(rand_callback),
        endpoint_backoff_(kMaxEndpointBackoffCacheSize) {
    DCHECK(policy);
    DCHECK(tick_clock);
    DCHECK(delegate);
    DCHECK(cache);
  }

  ReportingEndpointManagerImpl(const ReportingEndpointManagerImpl&) = delete;
  ReportingEndpointManagerImpl& operator=(const ReportingEndpointManagerImpl&) =
      delete;

  ~ReportingEndpointManagerImpl() override = default;

  const ReportingEndpoint FindEndpointForDelivery(
      const ReportingEndpointGroupKey& group_key) override {
    // Get unexpired endpoints that apply to a delivery to |origin| and |group|.
    // May have been configured by a superdomain of |origin|.
    std::vector<ReportingEndpoint> endpoints =
        cache_->GetCandidateEndpointsForDelivery(group_key);

    // Highest-priority endpoint(s) that are not expired, failing, or
    // forbidden for use by the ReportingDelegate.
    std::vector<ReportingEndpoint> available_endpoints;
    // Total weight of endpoints in |available_endpoints|.
    int total_weight = 0;

    for (const ReportingEndpoint& endpoint : endpoints) {
      // Enterprise endpoints don't have an origin.
      if (endpoint.group_key.target_type == ReportingTargetType::kDeveloper) {
        DCHECK(endpoint.group_key.origin.has_value());
        if (!delegate_->CanUseClient(endpoint.group_key.origin.value(),
                                     endpoint.info.url)) {
          continue;
        }
      }

      // If this client is lower priority than the ones we've found, skip it.
      if (!available_endpoints.empty() &&
          endpoint.info.priority > available_endpoints[0].info.priority) {
        continue;
      }

      // This brings each match to the front of the MRU cache, so if an entry
      // frequently matches requests, it's more likely to stay in the cache.
      auto endpoint_backoff_it = endpoint_backoff_.Get(EndpointBackoffKey(
          group_key.network_anonymization_key, endpoint.info.url));
      if (endpoint_backoff_it != endpoint_backoff_.end() &&
          endpoint_backoff_it->second->ShouldRejectRequest()) {
        continue;
      }

      // If this client is higher priority than the ones we've found (or we
      // haven't found any), forget about those ones and remember this one.
      if (available_endpoints.empty() ||
          endpoint.info.priority < available_endpoints[0].info.priority) {
        available_endpoints.clear();
        total_weight = 0;
      }

      available_endpoints.push_back(endpoint);
      total_weight += endpoint.info.weight;
    }

    if (available_endpoints.empty()) {
      return ReportingEndpoint();
    }

    if (total_weight == 0) {
      int random_index = rand_callback_.Run(0, available_endpoints.size() - 1);
      return available_endpoints[random_index];
    }

    int random_index = rand_callback_.Run(0, total_weight - 1);
    int weight_so_far = 0;
    for (const auto& endpoint : available_endpoints) {
      weight_so_far += endpoint.info.weight;
      if (random_index < weight_so_far) {
        return endpoint;
      }
    }

    // TODO(juliatuttle): Can we reach this in some weird overflow case?
    NOTREACHED_IN_MIGRATION();
    return ReportingEndpoint();
  }

  void InformOfEndpointRequest(
      const NetworkAnonymizationKey& network_anonymization_key,
      const GURL& endpoint,
      bool succeeded) override {
    EndpointBackoffKey endpoint_backoff_key(network_anonymization_key,
                                            endpoint);
    // This will bring the entry to the front of the cache, if it exists.
    auto endpoint_backoff_it = endpoint_backoff_.Get(endpoint_backoff_key);
    if (endpoint_backoff_it == endpoint_backoff_.end()) {
      endpoint_backoff_it = endpoint_backoff_.Put(
          std::move(endpoint_backoff_key),
          std::make_unique<BackoffEntry>(&policy_->endpoint_backoff_policy,
                                         tick_clock_));
    }
    endpoint_backoff_it->second->InformOfRequest(succeeded);
  }

 private:
  using EndpointBackoffKey = std::pair<NetworkAnonymizationKey, GURL>;

  const raw_ptr<const ReportingPolicy> policy_;
  const raw_ptr<const base::TickClock> tick_clock_;
  const raw_ptr<const ReportingDelegate> delegate_;
  const raw_ptr<ReportingCache> cache_;

  RandIntCallback rand_callback_;

  // Note: Currently the ReportingBrowsingDataRemover does not clear this data
  // because it's not persisted to disk. If it's ever persisted, it will need
  // to be cleared as well.
  // TODO(chlily): clear this data when endpoints are deleted to avoid unbounded
  // growth of this map.
  base::LRUCache<EndpointBackoffKey, std::unique_ptr<net::BackoffEntry>>
      endpoint_backoff_;
};

}  // namespace

// static
std::unique_ptr<ReportingEndpointManager> ReportingEndpointManager::Create(
    const ReportingPolicy* policy,
    const base::TickClock* tick_clock,
    const ReportingDelegate* delegate,
    ReportingCache* cache,
    const RandIntCallback& rand_callback) {
  return std::make_unique<ReportingEndpointManagerImpl>(
      policy, tick_clock, delegate, cache, rand_callback);
}

ReportingEndpointManager::~ReportingEndpointManager() = default;

}  // namespace net

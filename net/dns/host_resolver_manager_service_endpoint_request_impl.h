// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_MANAGER_SERVICE_ENDPOINT_REQUEST_IMPL_H_
#define NET_DNS_HOST_RESOLVER_MANAGER_SERVICE_ENDPOINT_REQUEST_IMPL_H_

#include <deque>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/linked_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/tick_clock.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_manager_job.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/resolve_context.h"
#include "net/log/net_log_with_source.h"
#include "url/scheme_host_port.h"

namespace net {

// Implementation of HostResolver::ServiceEndpointRequest.
class HostResolverManager::ServiceEndpointRequestImpl
    : public HostResolver::ServiceEndpointRequest,
      public base::LinkNode<HostResolverManager::ServiceEndpointRequestImpl> {
 public:
  ServiceEndpointRequestImpl(url::SchemeHostPort scheme_host_port,
                             NetworkAnonymizationKey network_anonymization_key,
                             NetLogWithSource net_log,
                             ResolveHostParameters parameters,
                             base::WeakPtr<ResolveContext> resolve_context,
                             base::WeakPtr<HostResolverManager> manager,
                             const base::TickClock* tick_clock);

  ServiceEndpointRequestImpl(const ServiceEndpointRequestImpl&) = delete;
  ServiceEndpointRequestImpl& operator=(const ServiceEndpointRequestImpl&) =
      delete;

  ~ServiceEndpointRequestImpl() override;

  // HostResolver::ServiceEndpointRequest implementations:
  int Start(Delegate* delegate) override;
  base::span<const ServiceEndpoint> GetEndpointResults() override;
  const std::set<std::string>& GetDnsAliasResults() override;
  bool EndpointsCryptoReady() override;
  ResolveErrorInfo GetResolveErrorInfo() override;
  const HostCache::EntryStaleness* GetStaleInfo() const override;
  bool IsStaleWhileRefresing() const override;
  void ChangeRequestPriority(RequestPriority priority) override;
  std::string DebugString() const override;

  // These should only be called from HostResolver::Job.
  void AssignJob(base::SafeRef<Job> job);
  void OnJobCompleted(const HostCache::Entry& results, bool obtained_securely);
  void OnJobCancelled();
  void OnServiceEndpointsChanged();

  const NetLogWithSource& net_log() const { return net_log_; }

  const ResolveHostParameters& parameters() const { return parameters_; }

  RequestPriority priority() const { return priority_; }
  void set_priority(RequestPriority priority) { priority_ = priority; }

  HostCache* host_cache() const {
    return resolve_context_ ? resolve_context_->host_cache() : nullptr;
  }

  base::WeakPtr<ServiceEndpointRequestImpl> GetWeakPtr();

 private:
  enum class State {
    kNone,
    kCheckIPv6Reachability,
    kCheckIPv6ReachabilityComplete,
    kDoResolveLocally,
    kStartJob,
  };

  int DoLoop(int rv);
  int DoCheckIPv6Reachability();
  int DoCheckIPv6ReachabilityComplete(int rv);
  int DoResolveLocally();
  int DoStartJob();

  void OnIOComplete(int rv);

  void SetFinalizedResultFromLegacyResults(const HostCache::Entry& results);

  void MaybeClearStaleResults();

  void LogCancelRequest();

  void NotifyDelegateOfUpdated();

  ClientSocketFactory* GetClientSocketFactory();

  State next_state_ = State::kNone;

  const HostResolver::Host host_;
  const NetworkAnonymizationKey network_anonymization_key_;
  const NetLogWithSource net_log_;
  ResolveHostParameters parameters_;
  base::WeakPtr<ResolveContext> resolve_context_;
  base::WeakPtr<HostResolverManager> manager_;
  const raw_ptr<const base::TickClock> tick_clock_;
  RequestPriority priority_;

  // `delegate_` must outlive `this` unless `resolve_context_` becomes invalid.
  raw_ptr<Delegate> delegate_;

  // Holds the finalized results.
  struct FinalizedResult {
    FinalizedResult(std::vector<ServiceEndpoint> endpoints,
                    std::set<std::string> dns_aliases);
    ~FinalizedResult();

    FinalizedResult(FinalizedResult&&);
    FinalizedResult& operator=(FinalizedResult&&);

    std::vector<ServiceEndpoint> endpoints;
    std::set<std::string> dns_aliases;
  };

  // Set when the endpoint results are finalized.
  std::optional<FinalizedResult> finalized_result_;

  // These fields are calculated by DoResolveLocally() and consumed by
  // DoStartJob().
  std::optional<JobKey> job_key_;
  std::deque<TaskType> tasks_;

  // These fields are set when the cache has stale results and `this` allows to
  // lookup the cache. Cleared upon receiving fresh results if `this` allows
  // stale results while refreshing.
  std::optional<HostCache::EntryStaleness> stale_info_;
  std::vector<ServiceEndpoint> stale_endpoints_;

  // Set when a job is associated with `this`. Must be valid unless
  // `resolve_context_` becomes invalid. Cleared when the endpoints are
  // finalized to ensure that `job_` doesn't become a dangling pointer.
  std::optional<base::SafeRef<Job>> job_;

  ResolveErrorInfo error_info_;

  std::vector<TaskType> initial_tasks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ServiceEndpointRequestImpl> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_MANAGER_SERVICE_ENDPOINT_REQUEST_IMPL_H_

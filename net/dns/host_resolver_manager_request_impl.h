// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_MANAGER_REQUEST_IMPL_H_
#define NET_DNS_HOST_RESOLVER_MANAGER_REQUEST_IMPL_H_

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
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/request_priority.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_manager_job.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/log/net_log_with_source.h"

namespace net {

class ClientSocketFactory;
class ResolveContext;

// Holds the callback and request parameters for an outstanding request.
//
// The RequestImpl is owned by the end user of host resolution. Deletion prior
// to the request having completed means the request was cancelled by the
// caller.
//
// Both the RequestImpl and its associated Job hold non-owning pointers to each
// other. Care must be taken to clear the corresponding pointer when
// cancellation is initiated by the Job (OnJobCancelled) vs by the end user
// (~RequestImpl).
class HostResolverManager::RequestImpl
    : public HostResolver::ResolveHostRequest,
      public base::LinkNode<HostResolverManager::RequestImpl> {
 public:
  RequestImpl(NetLogWithSource source_net_log,
              HostResolver::Host request_host,
              NetworkAnonymizationKey network_anonymization_key,
              std::optional<ResolveHostParameters> optional_parameters,
              base::WeakPtr<ResolveContext> resolve_context,
              base::WeakPtr<HostResolverManager> resolver,
              const base::TickClock* tick_clock);

  RequestImpl(const RequestImpl&) = delete;
  RequestImpl& operator=(const RequestImpl&) = delete;

  ~RequestImpl() override;

  // HostResolver::ResolveHostRequest implementations:
  int Start(CompletionOnceCallback callback) override;
  const AddressList* GetAddressResults() const override;
  const std::vector<HostResolverEndpointResult>* GetEndpointResults()
      const override;
  const std::vector<std::string>* GetTextResults() const override;
  const std::vector<HostPortPair>* GetHostnameResults() const override;
  const std::set<std::string>* GetDnsAliasResults() const override;
  const std::vector<bool>* GetExperimentalResultsForTesting() const override;
  net::ResolveErrorInfo GetResolveErrorInfo() const override;
  const std::optional<HostCache::EntryStaleness>& GetStaleInfo() const override;
  void ChangeRequestPriority(RequestPriority priority) override;

  void set_results(HostCache::Entry results);
  void set_error_info(int error, bool is_secure_network_error);
  void set_stale_info(HostCache::EntryStaleness stale_info);

  void AssignJob(base::SafeRef<Job> job);

  bool HasJob() const { return job_.has_value(); }

  // Gets the Job's key. Crashes if no Job has been assigned.
  const JobKey& GetJobKey() const;

  // Unassigns the Job without calling completion callback.
  void OnJobCancelled(const JobKey& key);

  // Cleans up Job assignment, marks request completed, and calls the completion
  // callback. |is_secure_network_error| indicates whether |error| came from a
  // secure DNS lookup.
  void OnJobCompleted(const JobKey& job_key,
                      int error,
                      bool is_secure_network_error);

  // NetLog for the source, passed in HostResolver::Resolve.
  const NetLogWithSource& source_net_log() { return source_net_log_; }

  const HostResolver::Host& request_host() const { return request_host_; }

  const NetworkAnonymizationKey& network_anonymization_key() const {
    return network_anonymization_key_;
  }

  const ResolveHostParameters& parameters() const { return parameters_; }

  ResolveContext* resolve_context() const { return resolve_context_.get(); }

  HostCache* host_cache() const {
    return resolve_context_ ? resolve_context_->host_cache() : nullptr;
  }

  RequestPriority priority() const { return priority_; }
  void set_priority(RequestPriority priority) { priority_ = priority; }

 private:
  enum ResolveState {
    STATE_IPV6_REACHABILITY,
    STATE_GET_PARAMETERS,
    STATE_GET_PARAMETERS_COMPLETE,
    STATE_RESOLVE_LOCALLY,
    STATE_START_JOB,
    STATE_FINISH_REQUEST,
    STATE_NONE,
  };

  int DoLoop(int rv);
  void OnIOComplete(int rv);
  int DoIPv6Reachability();
  int DoGetParameters();
  int DoGetParametersComplete(int rv);
  int DoResolveLocally();
  int DoStartJob();
  int DoFinishRequest(int rv);

  void FixUpEndpointAndAliasResults();

  // Logging and metrics for when a request has just been started.
  void LogStartRequest();

  // Logging and metrics for when a request has just completed (before its
  // callback is run).
  void LogFinishRequest(int net_error, bool async_completion);

  // Logs when a request has been cancelled.
  void LogCancelRequest();

  ClientSocketFactory* GetClientSocketFactory();

  const NetLogWithSource source_net_log_;

  const HostResolver::Host request_host_;
  const NetworkAnonymizationKey network_anonymization_key_;
  ResolveHostParameters parameters_;
  base::WeakPtr<ResolveContext> resolve_context_;

  RequestPriority priority_;

  ResolveState next_state_;
  JobKey job_key_;
  IPAddress ip_address_;

  std::deque<TaskType> tasks_;
  // The resolve job that this request is dependent on.
  std::optional<base::SafeRef<Job>> job_;
  base::WeakPtr<HostResolverManager> resolver_ = nullptr;

  // The user's callback to invoke when the request completes.
  CompletionOnceCallback callback_;

  bool complete_ = false;
  bool only_ipv6_reachable_ = false;
  std::optional<HostCache::Entry> results_;
  std::optional<HostCache::EntryStaleness> stale_info_;
  std::optional<AddressList> legacy_address_results_;
  std::optional<std::vector<HostResolverEndpointResult>> endpoint_results_;
  std::optional<std::set<std::string>> fixed_up_dns_alias_results_;
  ResolveErrorInfo error_info_;

  const raw_ptr<const base::TickClock> tick_clock_;
  base::TimeTicks request_time_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RequestImpl> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_MANAGER_REQUEST_IMPL_H_

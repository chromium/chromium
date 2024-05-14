// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_manager_request_impl.h"

#include <deque>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/linked_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/time/tick_clock.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/request_priority.h"
#include "net/dns/dns_alias_utility.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_manager_job.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/url_request/url_request_context.h"

namespace net {

HostResolverManager::RequestImpl::RequestImpl(
    NetLogWithSource source_net_log,
    HostResolver::Host request_host,
    NetworkAnonymizationKey network_anonymization_key,
    std::optional<ResolveHostParameters> optional_parameters,
    base::WeakPtr<ResolveContext> resolve_context,
    base::WeakPtr<HostResolverManager> resolver,
    const base::TickClock* tick_clock)
    : source_net_log_(std::move(source_net_log)),
      request_host_(std::move(request_host)),
      network_anonymization_key_(
          NetworkAnonymizationKey::IsPartitioningEnabled()
              ? std::move(network_anonymization_key)
              : NetworkAnonymizationKey()),
      parameters_(optional_parameters ? std::move(optional_parameters).value()
                                      : ResolveHostParameters()),
      resolve_context_(std::move(resolve_context)),
      priority_(parameters_.initial_priority),
      job_key_(request_host_, resolve_context_.get()),
      resolver_(std::move(resolver)),
      tick_clock_(tick_clock) {}

HostResolverManager::RequestImpl::~RequestImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!job_.has_value()) {
    return;
  }

  job_.value()->CancelRequest(this);
  LogCancelRequest();
}

int HostResolverManager::RequestImpl::Start(CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  // Start() may only be called once per request.
  CHECK(!job_.has_value());
  DCHECK(!complete_);
  DCHECK(!callback_);
  // Parent HostResolver must still be alive to call Start().
  DCHECK(resolver_);

  if (!resolve_context_) {
    complete_ = true;
    resolver_.reset();
    set_error_info(ERR_CONTEXT_SHUT_DOWN, false);
    return ERR_NAME_NOT_RESOLVED;
  }

  LogStartRequest();

  next_state_ = STATE_IPV6_REACHABILITY;
  callback_ = std::move(callback);

  int rv = OK;
  rv = DoLoop(rv);
  return rv;
}

const AddressList* HostResolverManager::RequestImpl::GetAddressResults() const {
  DCHECK(complete_);
  return base::OptionalToPtr(legacy_address_results_);
}

const std::vector<HostResolverEndpointResult>*
HostResolverManager::RequestImpl::GetEndpointResults() const {
  DCHECK(complete_);
  return base::OptionalToPtr(endpoint_results_);
}

const std::vector<std::string>*
HostResolverManager::RequestImpl::GetTextResults() const {
  DCHECK(complete_);
  return results_ ? &results_.value().text_records() : nullptr;
}

const std::vector<HostPortPair>*
HostResolverManager::RequestImpl::GetHostnameResults() const {
  DCHECK(complete_);
  return results_ ? &results_.value().hostnames() : nullptr;
}

const std::set<std::string>*
HostResolverManager::RequestImpl::GetDnsAliasResults() const {
  DCHECK(complete_);

  // If `include_canonical_name` param was true, should only ever have at most
  // a single alias, representing the expected "canonical name".
#if DCHECK_IS_ON()
  if (parameters().include_canonical_name && fixed_up_dns_alias_results_) {
    DCHECK_LE(fixed_up_dns_alias_results_->size(), 1u);
    if (GetAddressResults()) {
      std::set<std::string> address_list_aliases_set(
          GetAddressResults()->dns_aliases().begin(),
          GetAddressResults()->dns_aliases().end());
      DCHECK(address_list_aliases_set == fixed_up_dns_alias_results_.value());
    }
  }
#endif  // DCHECK_IS_ON()

  return base::OptionalToPtr(fixed_up_dns_alias_results_);
}

const std::vector<bool>*
HostResolverManager::RequestImpl::GetExperimentalResultsForTesting() const {
  DCHECK(complete_);
  return results_ ? &results_.value().https_record_compatibility() : nullptr;
}

net::ResolveErrorInfo HostResolverManager::RequestImpl::GetResolveErrorInfo()
    const {
  DCHECK(complete_);
  return error_info_;
}

const std::optional<HostCache::EntryStaleness>&
HostResolverManager::RequestImpl::GetStaleInfo() const {
  DCHECK(complete_);
  return stale_info_;
}

void HostResolverManager::RequestImpl::ChangeRequestPriority(
    RequestPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!job_.has_value()) {
    priority_ = priority;
    return;
  }
  job_.value()->ChangeRequestPriority(this, priority);
}

void HostResolverManager::RequestImpl::set_results(HostCache::Entry results) {
  // Should only be called at most once and before request is marked
  // completed.
  DCHECK(!complete_);
  DCHECK(!results_);
  DCHECK(!parameters_.is_speculative);

  results_ = std::move(results);
  FixUpEndpointAndAliasResults();
}

void HostResolverManager::RequestImpl::set_error_info(
    int error,
    bool is_secure_network_error) {
  error_info_ = ResolveErrorInfo(error, is_secure_network_error);
}

void HostResolverManager::RequestImpl::set_stale_info(
    HostCache::EntryStaleness stale_info) {
  // Should only be called at most once and before request is marked
  // completed.
  DCHECK(!complete_);
  DCHECK(!stale_info_);
  DCHECK(!parameters_.is_speculative);

  stale_info_ = std::move(stale_info);
}

void HostResolverManager::RequestImpl::AssignJob(base::SafeRef<Job> job) {
  CHECK(!job_.has_value());
  job_ = std::move(job);
}

const HostResolverManager::JobKey& HostResolverManager::RequestImpl::GetJobKey()
    const {
  CHECK(job_.has_value());
  return job_.value()->key();
}

void HostResolverManager::RequestImpl::OnJobCancelled(const JobKey& job_key) {
  CHECK(job_.has_value());
  CHECK(job_key == job_.value()->key());
  job_.reset();
  DCHECK(!complete_);
  DCHECK(callback_);
  callback_.Reset();

  // No results should be set.
  DCHECK(!results_);

  LogCancelRequest();
}

void HostResolverManager::RequestImpl::OnJobCompleted(
    const JobKey& job_key,
    int error,
    bool is_secure_network_error) {
  set_error_info(error, is_secure_network_error);

  CHECK(job_.has_value());
  CHECK(job_key == job_.value()->key());
  job_.reset();

  DCHECK(!complete_);
  complete_ = true;

  LogFinishRequest(error, true /* async_completion */);

  DCHECK(callback_);
  std::move(callback_).Run(HostResolver::SquashErrorCode(error));
}

int HostResolverManager::RequestImpl::DoLoop(int rv) {
  do {
    ResolveState state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_IPV6_REACHABILITY:
        rv = DoIPv6Reachability();
        break;
      case STATE_GET_PARAMETERS:
        DCHECK_EQ(OK, rv);
        rv = DoGetParameters();
        break;
      case STATE_GET_PARAMETERS_COMPLETE:
        rv = DoGetParametersComplete(rv);
        break;
      case STATE_RESOLVE_LOCALLY:
        rv = DoResolveLocally();
        break;
      case STATE_START_JOB:
        rv = DoStartJob();
        break;
      case STATE_FINISH_REQUEST:
        rv = DoFinishRequest(rv);
        break;
      default:
        NOTREACHED_IN_MIGRATION() << "next_state_: " << next_state_;
        break;
    }
  } while (next_state_ != STATE_NONE && rv != ERR_IO_PENDING);

  return rv;
}

void HostResolverManager::RequestImpl::OnIOComplete(int rv) {
  rv = DoLoop(rv);
  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    std::move(callback_).Run(rv);
  }
}

int HostResolverManager::RequestImpl::DoIPv6Reachability() {
  next_state_ = STATE_GET_PARAMETERS;
  // If a single reachability probe has not been completed, and the latest
  // probe will return asynchronously, return ERR_NAME_NOT_RESOLVED when the
  // request source is LOCAL_ONLY. This is due to LOCAL_ONLY requiring a
  // synchronous response, so it cannot wait on an async probe result and
  // cannot make assumptions about reachability.
  if (parameters_.source == HostResolverSource::LOCAL_ONLY) {
    int rv = resolver_->StartIPv6ReachabilityCheck(
        source_net_log_, GetClientSocketFactory(),
        base::DoNothingAs<void(int)>());
    if (rv == ERR_IO_PENDING) {
      next_state_ = STATE_FINISH_REQUEST;
      return ERR_NAME_NOT_RESOLVED;
    }
    return OK;
  }
  return resolver_->StartIPv6ReachabilityCheck(
      source_net_log_, GetClientSocketFactory(),
      base::BindOnce(&RequestImpl::OnIOComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

int HostResolverManager::RequestImpl::DoGetParameters() {
  resolver_->InitializeJobKeyAndIPAddress(network_anonymization_key_,
                                          parameters_, source_net_log_,
                                          job_key_, ip_address_);

  // A reachability probe to determine if the network is only reachable on
  // IPv6 will be scheduled if the parameters are met for using NAT64 in place
  // of an IPv4 address.
  if (HostResolver::MayUseNAT64ForIPv4Literal(
          job_key_.flags, parameters_.source, ip_address_) &&
      resolver_->last_ipv6_probe_result_) {
    next_state_ = STATE_GET_PARAMETERS_COMPLETE;
    return resolver_->StartGloballyReachableCheck(
        ip_address_, source_net_log_, GetClientSocketFactory(),
        base::BindOnce(&RequestImpl::OnIOComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  next_state_ = STATE_RESOLVE_LOCALLY;
  return OK;
}

int HostResolverManager::RequestImpl::DoGetParametersComplete(int rv) {
  next_state_ = STATE_RESOLVE_LOCALLY;
  only_ipv6_reachable_ = (rv == ERR_FAILED) ? true : false;
  return OK;
}

int HostResolverManager::RequestImpl::DoResolveLocally() {
  std::optional<HostCache::EntryStaleness> stale_info;
  HostCache::Entry results = resolver_->ResolveLocally(
      only_ipv6_reachable_, job_key_, ip_address_, parameters_.cache_usage,
      parameters_.secure_dns_policy, parameters_.source, source_net_log_,
      host_cache(), &tasks_, &stale_info);
  if (results.error() != ERR_DNS_CACHE_MISS ||
      parameters_.source == HostResolverSource::LOCAL_ONLY || tasks_.empty()) {
    if (results.error() == OK && !parameters_.is_speculative) {
      set_results(results.CopyWithDefaultPort(request_host_.GetPort()));
    }
    if (stale_info && !parameters_.is_speculative) {
      set_stale_info(std::move(stale_info).value());
    }
    next_state_ = STATE_FINISH_REQUEST;
    return results.error();
  }
  next_state_ = STATE_START_JOB;
  return OK;
}

int HostResolverManager::RequestImpl::DoStartJob() {
  resolver_->CreateAndStartJob(std::move(job_key_), std::move(tasks_), this);
  DCHECK(!complete_);
  resolver_.reset();
  return ERR_IO_PENDING;
}

int HostResolverManager::RequestImpl::DoFinishRequest(int rv) {
  CHECK(!job_.has_value());
  complete_ = true;
  set_error_info(rv, /*is_secure_network_error=*/false);
  rv = HostResolver::SquashErrorCode(rv);
  LogFinishRequest(rv, /*async_completion=*/false);
  return rv;
}

void HostResolverManager::RequestImpl::FixUpEndpointAndAliasResults() {
  DCHECK(results_.has_value());
  DCHECK(!legacy_address_results_.has_value());
  DCHECK(!endpoint_results_.has_value());
  DCHECK(!fixed_up_dns_alias_results_.has_value());

  endpoint_results_ = results_.value().GetEndpoints();
  if (endpoint_results_.has_value()) {
    fixed_up_dns_alias_results_ = results_.value().aliases();

    // Skip fixups for `include_canonical_name` requests. Just use the
    // canonical name exactly as it was received from the system resolver.
    if (parameters().include_canonical_name) {
      DCHECK_LE(fixed_up_dns_alias_results_.value().size(), 1u);
    } else {
      fixed_up_dns_alias_results_ = dns_alias_utility::FixUpDnsAliases(
          fixed_up_dns_alias_results_.value());
    }

    legacy_address_results_ = HostResolver::EndpointResultToAddressList(
        endpoint_results_.value(), fixed_up_dns_alias_results_.value());
  }
}

void HostResolverManager::RequestImpl::LogStartRequest() {
  DCHECK(request_time_.is_null());
  request_time_ = tick_clock_->NowTicks();

  source_net_log_.BeginEvent(
      NetLogEventType::HOST_RESOLVER_MANAGER_REQUEST, [this] {
        base::Value::Dict dict;
        dict.Set("host", request_host_.ToString());
        dict.Set("dns_query_type",
                 kDnsQueryTypes.at(parameters_.dns_query_type));
        dict.Set("allow_cached_response",
                 parameters_.cache_usage !=
                     ResolveHostParameters::CacheUsage::DISALLOWED);
        dict.Set("is_speculative", parameters_.is_speculative);
        dict.Set("network_anonymization_key",
                 network_anonymization_key_.ToDebugString());
        dict.Set("secure_dns_policy",
                 base::strict_cast<int>(parameters_.secure_dns_policy));
        return dict;
      });
}

void HostResolverManager::RequestImpl::LogFinishRequest(int net_error,
                                                        bool async_completion) {
  source_net_log_.EndEventWithNetErrorCode(
      NetLogEventType::HOST_RESOLVER_MANAGER_REQUEST, net_error);

  if (!parameters_.is_speculative) {
    DCHECK(!request_time_.is_null());
    base::TimeDelta duration = tick_clock_->NowTicks() - request_time_;

    UMA_HISTOGRAM_MEDIUM_TIMES("Net.DNS.Request.TotalTime", duration);
    if (async_completion) {
      UMA_HISTOGRAM_MEDIUM_TIMES("Net.DNS.Request.TotalTimeAsync", duration);
    }
  }
}

void HostResolverManager::RequestImpl::LogCancelRequest() {
  source_net_log_.AddEvent(NetLogEventType::CANCELLED);
  source_net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_MANAGER_REQUEST);
}

ClientSocketFactory*
HostResolverManager::RequestImpl::GetClientSocketFactory() {
  if (resolve_context_->url_request_context()) {
    return resolve_context_->url_request_context()
        ->GetNetworkSessionContext()
        ->client_socket_factory;
  } else {
    return ClientSocketFactory::GetDefaultFactory();
  }
}

}  // namespace net

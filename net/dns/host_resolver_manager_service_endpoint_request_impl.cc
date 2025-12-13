// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_manager_service_endpoint_request_impl.h"

#include <sstream>

#include "base/containers/to_vector.h"
#include "base/memory/safe_ref.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/types/optional_util.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_alias_utility.h"
#include "net/dns/dns_task_results_manager.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_network_session.h"
#include "net/socket/client_socket_factory.h"
#include "net/url_request/url_request_context.h"
#include "url/scheme_host_port.h"

namespace net {

HostResolverManager::ServiceEndpointRequestImpl::FinalizedResult::
    FinalizedResult(std::vector<ServiceEndpoint> endpoints,
                    std::set<std::string> dns_aliases)
    : endpoints(std::move(endpoints)), dns_aliases(std::move(dns_aliases)) {}

HostResolverManager::ServiceEndpointRequestImpl::FinalizedResult::
    ~FinalizedResult() = default;

HostResolverManager::ServiceEndpointRequestImpl::FinalizedResult::
    FinalizedResult(FinalizedResult&&) = default;
HostResolverManager::ServiceEndpointRequestImpl::FinalizedResult&
HostResolverManager::ServiceEndpointRequestImpl::FinalizedResult::operator=(
    FinalizedResult&&) = default;

HostResolverManager::ServiceEndpointRequestImpl::ServiceEndpointRequestImpl(
    url::SchemeHostPort scheme_host_port,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    ResolveHostParameters parameters,
    base::WeakPtr<ResolveContext> resolve_context,
    base::WeakPtr<HostResolverManager> manager,
    const base::TickClock* tick_clock)
    : host_(std::move(scheme_host_port)),
      network_anonymization_key_(
          NetworkAnonymizationKey::IsPartitioningEnabled()
              ? std::move(network_anonymization_key)
              : NetworkAnonymizationKey()),
      net_log_(std::move(net_log)),
      parameters_(std::move(parameters)),
      resolve_context_(std::move(resolve_context)),
      manager_(std::move(manager)),
      tick_clock_(tick_clock),
      priority_(parameters_.initial_priority) {}

HostResolverManager::ServiceEndpointRequestImpl::~ServiceEndpointRequestImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!job_.has_value()) {
    return;
  }

  LogCancelRequest();

  // Clear the delegate to avoid calling delegate's callback after destruction.
  // The following CancelServiceEndpointRequest() could result in calling
  // OnJobCancelled() synchronously.
  delegate_ = nullptr;

  job_.value()->CancelServiceEndpointRequest(this);
  // TODO(crbug.com/397597592): Remove the following CHECKs after we identified
  // the cause of the bug.
  CHECK(previous() == nullptr);
  CHECK(next() == nullptr);
}

int HostResolverManager::ServiceEndpointRequestImpl::Start(Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!delegate_);
  CHECK(manager_);

  if (!resolve_context_) {
    error_info_ = ResolveErrorInfo(ERR_CONTEXT_SHUT_DOWN);
    return ERR_CONTEXT_SHUT_DOWN;
  }

  delegate_ = delegate;

  next_state_ = State::kCheckIPv6Reachability;
  return DoLoop(OK);
}

const HostCache::EntryStaleness*
HostResolverManager::ServiceEndpointRequestImpl::GetStaleInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::OptionalToPtr(stale_info_);
}

bool HostResolverManager::ServiceEndpointRequestImpl::IsStaleWhileRefresing()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return parameters_.cache_usage == ResolveHostParameters::CacheUsage::
                                        STALE_ALLOWED_WHILE_REFRESHING &&
         stale_info_.has_value() && stale_info_.value().is_stale();
}

base::span<const ServiceEndpoint>
HostResolverManager::ServiceEndpointRequestImpl::GetEndpointResults() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (finalized_result_.has_value()) {
    return finalized_result_->endpoints;
  }

  // There are two cases where `stale_endpoints_` is empty:
  //  * No stale results received yet.
  //  * The stale result is negative.
  // In either case, providing stale results isn't useful, so provide stale
  // results only if it's not empty.
  if (!stale_endpoints_.empty()) {
    return stale_endpoints_;
  }

  if (job_ && job_.value()->dns_task_results_manager()) {
    return job_.value()->dns_task_results_manager()->GetCurrentEndpoints();
  }

  return {};
}

const std::set<std::string>&
HostResolverManager::ServiceEndpointRequestImpl::GetDnsAliasResults() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (finalized_result_.has_value()) {
    return finalized_result_->dns_aliases;
  }

  if (job_ && job_.value()->dns_task_results_manager()) {
    return job_.value()->dns_task_results_manager()->GetAliases();
  }

  static const base::NoDestructor<std::set<std::string>> kEmptyDnsAliases;
  return *kEmptyDnsAliases.get();
}

bool HostResolverManager::ServiceEndpointRequestImpl::EndpointsCryptoReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (finalized_result_.has_value()) {
    // If there are no endpoints in the finalized result, `this` is not ready
    // for cryptographic handshakes.
    return !finalized_result_->endpoints.empty();
  }

  if (job_ && job_.value()->dns_task_results_manager()) {
    return job_.value()->dns_task_results_manager()->IsMetadataReady();
  }

  // If there is no running DnsTask, `this` is not ready for cryptographic
  // handshakes until receiving the final results.
  return false;
}

ResolveErrorInfo
HostResolverManager::ServiceEndpointRequestImpl::GetResolveErrorInfo() {
  return error_info_;
}

void HostResolverManager::ServiceEndpointRequestImpl::ChangeRequestPriority(
    RequestPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!job_.has_value()) {
    priority_ = priority;
    return;
  }
  job_.value()->ChangeServiceEndpointRequestPriority(this, priority);
}

std::string HostResolverManager::ServiceEndpointRequestImpl::DebugString()
    const {
  std::stringstream ss;
  ss << "it=[";
  for (const auto& task : initial_tasks_) {
    ss << base::strict_cast<int>(task) << ",";
  }
  ss << "],j=" << job_.has_value();
  if (job_) {
    ss << ",rm=" << (!!job_.value()->dns_task_results_manager());
  }
  return ss.str();
}

void HostResolverManager::ServiceEndpointRequestImpl::AssignJob(
    base::SafeRef<Job> job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!job_.has_value());
  job_ = job;
}

void HostResolverManager::ServiceEndpointRequestImpl::OnJobCompleted(
    const HostCache::Entry& results,
    bool obtained_securely) {
  CHECK(job_);
  CHECK(delegate_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  job_.reset();
  SetFinalizedResultFromLegacyResults(results);
  MaybeClearStaleResults();

  const bool is_secure_network_error =
      obtained_securely && results.error() != OK;
  error_info_ = ResolveErrorInfo(results.error(), is_secure_network_error);
  delegate_->OnServiceEndpointRequestFinished(
      HostResolver::SquashErrorCode(results.error()));
  // Do not add code below. `this` may be deleted at this point.
}

void HostResolverManager::ServiceEndpointRequestImpl::OnJobCancelled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(job_);

  job_.reset();

  // The owner of `this` has already destroyed `this`.
  if (!delegate_) {
    return;
  }

  LogCancelRequest();

  finalized_result_ = FinalizedResult(/*endpoints=*/{}, /*dns_aliases=*/{});
  error_info_ = ResolveErrorInfo(ERR_DNS_REQUEST_CANCELLED);
  delegate_->OnServiceEndpointRequestFinished(
      HostResolver::SquashErrorCode(ERR_DNS_REQUEST_CANCELLED));
  // Do not add code below. `this` may be deleted at this point.
}

void HostResolverManager::ServiceEndpointRequestImpl::
    OnServiceEndpointsChanged() {
  // This method is called asynchronously via a posted task. `job_` could
  // be completed or cancelled before executing the task.
  if (finalized_result_.has_value()) {
    return;
  }

  // There are fresh endpoints available. Clear stale endpoints and info if this
  // request allows stale results while refreshing.
  MaybeClearStaleResults();

  CHECK(job_);
  CHECK(delegate_);
  delegate_->OnServiceEndpointsUpdated();
  // Do not add code below. `this` may be deleted at this point.
}

base::WeakPtr<HostResolverManager::ServiceEndpointRequestImpl>
HostResolverManager::ServiceEndpointRequestImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

int HostResolverManager::ServiceEndpointRequestImpl::DoLoop(int rv) {
  do {
    State state = next_state_;
    next_state_ = State::kNone;
    switch (state) {
      case State::kCheckIPv6Reachability:
        rv = DoCheckIPv6Reachability();
        break;
      case State::kCheckIPv6ReachabilityComplete:
        rv = DoCheckIPv6ReachabilityComplete(rv);
        break;
      case State::kDoResolveLocally:
        rv = DoResolveLocally();
        break;
      case State::kStartJob:
        rv = DoStartJob();
        break;
      case State::kNone:
        NOTREACHED() << "Invalid state";
    }
  } while (next_state_ != State::kNone && rv != ERR_IO_PENDING);

  return rv;
}

int HostResolverManager::ServiceEndpointRequestImpl::DoCheckIPv6Reachability() {
  next_state_ = State::kCheckIPv6ReachabilityComplete;
  // LOCAL_ONLY requires a synchronous response, so it cannot wait on an async
  // reachability check result and cannot make assumptions about reachability.
  // Return ERR_NAME_NOT_RESOLVED when LOCAL_ONLY is specified and the check
  // is blocked. See also the comment in
  // HostResolverManager::RequestImpl::DoIPv6Reachability().
  if (parameters_.source == HostResolverSource::LOCAL_ONLY) {
    int rv = manager_->StartIPv6ReachabilityCheck(
        net_log_, GetClientSocketFactory(), base::DoNothingAs<void(int)>());
    if (rv == ERR_IO_PENDING) {
      next_state_ = State::kNone;
      finalized_result_ = FinalizedResult(/*endpoints=*/{}, /*dns_aliases=*/{});
      error_info_ = ResolveErrorInfo(ERR_NAME_NOT_RESOLVED);
      return ERR_NAME_NOT_RESOLVED;
    }
    return OK;
  }
  return manager_->StartIPv6ReachabilityCheck(
      net_log_, GetClientSocketFactory(),
      base::BindOnce(&ServiceEndpointRequestImpl::OnIOComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

int HostResolverManager::ServiceEndpointRequestImpl::
    DoCheckIPv6ReachabilityComplete(int rv) {
  next_state_ = rv == OK ? State::kDoResolveLocally : State::kNone;
  return rv;
}

int HostResolverManager::ServiceEndpointRequestImpl::DoResolveLocally() {
  job_key_ = JobKey(host_, resolve_context_.get());
  IPAddress ip_address;
  manager_->InitializeJobKeyAndIPAddress(
      network_anonymization_key_, parameters_, net_log_, *job_key_, ip_address);

  const bool only_ipv6_reachable = false;
  const bool stale_allowed_while_refreshing =
      parameters_.cache_usage ==
      ResolveHostParameters::CacheUsage::STALE_ALLOWED_WHILE_REFRESHING;

  // HostResolverManager doesn't recognize STALE_ALLOWED_WHILE_REFRESHING. This
  // class implements stale-while-refreshing logic (see the following comments).
  // Use ALLOWED when the parameter's the source is LOCAL_ONLY. Otherwise, use
  // STALE_ALLOWED to provide stale results as intermediate results.
  ResolveHostParameters::CacheUsage cache_usage = parameters_.cache_usage;
  if (stale_allowed_while_refreshing) {
    cache_usage = parameters_.source == HostResolverSource::LOCAL_ONLY
                      ? ResolveHostParameters::CacheUsage::ALLOWED
                      : ResolveHostParameters::CacheUsage::STALE_ALLOWED;
  }

  HostCache::Entry results = manager_->ResolveLocally(
      only_ipv6_reachable, *job_key_, ip_address, cache_usage,
      parameters_.secure_dns_policy, parameters_.source, net_log_, host_cache(),
      &tasks_, &stale_info_);
  bool is_stale = results.error() == OK && stale_info_.has_value() &&
                  stale_info_->is_stale();

  if (is_stale && stale_allowed_while_refreshing) {
    // When a stale result is found, ResolveLocally() returns the stale result
    // without executing the remaining tasks, including local tasks such as
    // INSECURE_CACHE_LOOKUP and HOSTS. These tasks may be able to provide a
    // fresh result, and are always expected to be tried (and removed from
    // `tasks_`) before starting an async Job. Call ResolveLocally() again with
    // CacheUsage::ALLOWED to see we can get a fresh result.
    // TODO(crbug.com/383174960): Consider refactoring ResolveLocally() so that
    // we don't have to call ResolveLocally() twice.
    CHECK_EQ(cache_usage, ResolveHostParameters::CacheUsage::STALE_ALLOWED);
    tasks_.clear();
    std::optional<HostCache::EntryStaleness> maybe_fresh_info;
    HostCache::Entry maybe_non_stale_results = manager_->ResolveLocally(
        only_ipv6_reachable, *job_key_, ip_address,
        ResolveHostParameters::CacheUsage::ALLOWED,
        parameters_.secure_dns_policy, parameters_.source, net_log_,
        host_cache(), &tasks_, &maybe_fresh_info);
    CHECK(!maybe_fresh_info.has_value() || !maybe_fresh_info->is_stale());
    if (maybe_non_stale_results.error() != ERR_DNS_CACHE_MISS ||
        tasks_.empty()) {
      stale_info_ = maybe_fresh_info;
      results = std::move(maybe_non_stale_results);
      is_stale = false;
    }
    CHECK(parameters_.source != HostResolverSource::LOCAL_ONLY);
  }

  if (is_stale && stale_allowed_while_refreshing) {
    // Allow using stale results only when there is no network change.
    // TODO(crbug.com/383174960): This also exclude results that are obtained
    // from the same network but the device got disconnected/connected events.
    // Ideally we should be able to use such results.
    if (results.network_changes() == host_cache()->network_changes()) {
      stale_endpoints_ = results.ConvertToServiceEndpoints(host_.GetPort());
    }
    if (!stale_endpoints_.empty()) {
      net_log_.AddEvent(
          NetLogEventType::HOST_RESOLVER_SERVICE_ENDPOINTS_STALE_RESULTS, [&] {
            base::Value::List endpoints;
            for (const auto& endpoint : stale_endpoints_) {
              endpoints.Append(endpoint.ToValue());
            }
            return base::Value::Dict().Set("endpoints", std::move(endpoints));
          });

      // Notify delegate of stale results asynchronously because notifying
      // delegate may delete `this`.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&ServiceEndpointRequestImpl::NotifyDelegateOfUpdated,
                         weak_ptr_factory_.GetWeakPtr()));
    }
    CHECK(!tasks_.empty());
  } else if (results.error() != ERR_DNS_CACHE_MISS ||
             parameters_.source == HostResolverSource::LOCAL_ONLY ||
             tasks_.empty()) {
    SetFinalizedResultFromLegacyResults(results);
    error_info_ = ResolveErrorInfo(results.error());
    return results.error();
  }

  next_state_ = State::kStartJob;
  return OK;
}

int HostResolverManager::ServiceEndpointRequestImpl::DoStartJob() {
  initial_tasks_ = base::ToVector(tasks_);
  manager_->CreateAndStartJobForServiceEndpointRequest(std::move(*job_key_),
                                                       std::move(tasks_), this);
  return ERR_IO_PENDING;
}

void HostResolverManager::ServiceEndpointRequestImpl::OnIOComplete(int rv) {
  DoLoop(rv);
}

void HostResolverManager::ServiceEndpointRequestImpl::
    SetFinalizedResultFromLegacyResults(const HostCache::Entry& results) {
  CHECK(!finalized_result_);
  if (results.error() == OK && !parameters_.is_speculative) {
    std::vector<ServiceEndpoint> endpoints =
        results.ConvertToServiceEndpoints(host_.GetPort());
    finalized_result_ =
        FinalizedResult(std::move(endpoints),
                        dns_alias_utility::FixUpDnsAliases(results.aliases()));
  } else {
    finalized_result_ = FinalizedResult(/*endpoints=*/{}, /*dns_aliases=*/{});
  }
}

void HostResolverManager::ServiceEndpointRequestImpl::MaybeClearStaleResults() {
  if (parameters_.cache_usage ==
          ResolveHostParameters::CacheUsage::STALE_ALLOWED_WHILE_REFRESHING &&
      stale_info_.has_value()) {
    stale_endpoints_.clear();
    stale_info_.reset();
  }
}

void HostResolverManager::ServiceEndpointRequestImpl::LogCancelRequest() {
  net_log_.AddEvent(NetLogEventType::CANCELLED);
  net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_MANAGER_REQUEST);
}

void HostResolverManager::ServiceEndpointRequestImpl::
    NotifyDelegateOfUpdated() {
  // This method is called asynchronously via a posted task. `job_` could
  // be completed or cancelled before executing the task.
  if (finalized_result_.has_value()) {
    return;
  }

  CHECK(job_);
  CHECK(delegate_);
  delegate_->OnServiceEndpointsUpdated();
  // Do not add code below. `this` may be deleted at this point.
}

ClientSocketFactory*
HostResolverManager::ServiceEndpointRequestImpl::GetClientSocketFactory() {
  if (resolve_context_->url_request_context()) {
    return resolve_context_->url_request_context()
        ->GetNetworkSessionContext()
        ->client_socket_factory;
  }
  return ClientSocketFactory::GetDefaultFactory();
}

}  // namespace net

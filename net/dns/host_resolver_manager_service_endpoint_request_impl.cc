// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_manager_service_endpoint_request_impl.h"

#include "base/memory/safe_ref.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/types/optional_util.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_alias_utility.h"
#include "net/dns/dns_task_results_manager.h"
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

const std::vector<ServiceEndpoint>&
HostResolverManager::ServiceEndpointRequestImpl::GetEndpointResults() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (finalized_result_.has_value()) {
    return finalized_result_->endpoints;
  }

  if (job_ && job_.value()->dns_task_results_manager()) {
    return job_.value()->dns_task_results_manager()->GetCurrentEndpoints();
  }

  static const base::NoDestructor<std::vector<ServiceEndpoint>> kEmptyEndpoints;
  return *kEmptyEndpoints.get();
}

const std::set<std::string>&
HostResolverManager::ServiceEndpointRequestImpl::GetDnsAliasResults() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (finalized_result_.has_value()) {
    return finalized_result_->dns_aliases;
  }

  if (job_ && job_.value()->dns_task_results_manager()) {
    // TODO(crbug.com/41493696): Call dns_alias_utility::FixUpDnsAliases().
    return job_.value()->dns_task_results_manager()->GetAliases();
  }

  static const base::NoDestructor<std::set<std::string>> kEmptyDnsAliases;
  return *kEmptyDnsAliases.get();
}

bool HostResolverManager::ServiceEndpointRequestImpl::EndpointsCryptoReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (finalized_result_.has_value()) {
    return true;
  }

  if (job_ && job_.value()->dns_task_results_manager()) {
    return job_.value()->dns_task_results_manager()->IsMetadataReady();
  }

  return true;
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

  // Try to resolve locally first.
  std::optional<HostCache::EntryStaleness> stale_info;
  HostCache::Entry results = manager_->ResolveLocally(
      /*only_ipv6_reachable=*/false, *job_key_, ip_address,
      parameters_.cache_usage, parameters_.secure_dns_policy,
      parameters_.source, net_log_, host_cache(), &tasks_, &stale_info);
  if (results.error() != ERR_DNS_CACHE_MISS ||
      parameters_.source == HostResolverSource::LOCAL_ONLY || tasks_.empty()) {
    SetFinalizedResultFromLegacyResults(results);
    error_info_ = ResolveErrorInfo(results.error());
    return results.error();
  }

  next_state_ = State::kStartJob;
  return OK;
}

int HostResolverManager::ServiceEndpointRequestImpl::DoStartJob() {
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

void HostResolverManager::ServiceEndpointRequestImpl::LogCancelRequest() {
  net_log_.AddEvent(NetLogEventType::CANCELLED);
  net_log_.EndEvent(NetLogEventType::HOST_RESOLVER_MANAGER_REQUEST);
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

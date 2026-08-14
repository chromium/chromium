// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_pool_async_dns_job.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "net/base/ech_mode.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/quic/quic_session_pool_endpoint_connector.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

QuicSessionPool::AsyncDnsJob::UsableEndpoint::UsableEndpoint(
    ServiceEndpoint endpoint,
    quic::ParsedQuicVersion quic_version)
    : endpoint(std::move(endpoint)), quic_version(quic_version) {}

QuicSessionPool::AsyncDnsJob::UsableEndpoint::~UsableEndpoint() = default;

QuicSessionPool::AsyncDnsJob::UsableEndpoint::UsableEndpoint(
    const UsableEndpoint&) = default;
QuicSessionPool::AsyncDnsJob::UsableEndpoint&
QuicSessionPool::AsyncDnsJob::UsableEndpoint::operator=(const UsableEndpoint&) =
    default;
QuicSessionPool::AsyncDnsJob::UsableEndpoint::UsableEndpoint(UsableEndpoint&&) =
    default;
QuicSessionPool::AsyncDnsJob::UsableEndpoint&
QuicSessionPool::AsyncDnsJob::UsableEndpoint::operator=(UsableEndpoint&&) =
    default;

QuicSessionPool::AsyncDnsJob::AttemptParams::AttemptParams() = default;
QuicSessionPool::AsyncDnsJob::AttemptParams::~AttemptParams() = default;

QuicSessionPool::AsyncDnsJob::AttemptParams::AttemptParams(
    const AttemptParams&) = default;
QuicSessionPool::AsyncDnsJob::AttemptParams&
QuicSessionPool::AsyncDnsJob::AttemptParams::operator=(const AttemptParams&) =
    default;
QuicSessionPool::AsyncDnsJob::AttemptParams::AttemptParams(AttemptParams&&) =
    default;
QuicSessionPool::AsyncDnsJob::AttemptParams&
QuicSessionPool::AsyncDnsJob::AttemptParams::operator=(AttemptParams&&) =
    default;

QuicSessionPool::AsyncDnsJob::AsyncDnsJob(
    QuicSessionPool* pool,
    quic::ParsedQuicVersion quic_version,
    HostResolver* host_resolver,
    QuicSessionAliasKey key,
    std::unique_ptr<CryptoClientConfigHandle> client_config_handle,
    bool retry_on_alternate_network_before_handshake,
    RequestPriority priority,
    bool use_dns_aliases,
    bool require_dns_https_alpn,
    int cert_verify_flags,
    MultiplexedSessionCreationInitiator session_creation_initiator,
    QuicSessionEstablishmentReason quic_session_establishment_reason,
    std::optional<ConnectionManagementConfig> connection_management_config,
    const NetLogWithSource& net_log)
    : Job(pool,
          std::move(key),
          std::move(client_config_handle),
          priority,
          quic_session_establishment_reason,
          NetLogWithSource::Make(
              net_log.net_log(),
              NetLogSourceType::QUIC_SESSION_POOL_ASYNC_DNS_JOB)),
      quic_version_(quic_version),
      host_resolver_(host_resolver),
      use_dns_aliases_(use_dns_aliases),
      require_dns_https_alpn_(require_dns_https_alpn),
      cert_verify_flags_(cert_verify_flags),
      retry_on_alternate_network_before_handshake_(
          retry_on_alternate_network_before_handshake),
      session_creation_initiator_(session_creation_initiator),
      connection_management_config_(connection_management_config) {
  CHECK_EQ(quic_version_.IsKnown(), !require_dns_https_alpn_);
}

QuicSessionPool::AsyncDnsJob::~AsyncDnsJob() = default;

int QuicSessionPool::AsyncDnsJob::Run(CompletionOnceCallback callback) {
  int rv = DoResolveHost();
  if (rv != ERR_IO_PENDING) {
    rv = DoResolveHostComplete(rv);
    // Resolution completed synchronously, before any request attached. The
    // host resolution signal has no receiver and must not be promised.
    host_resolution_notified_ = true;
  }
  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  }
  return rv > 0 ? OK : rv;
}

void QuicSessionPool::AsyncDnsJob::SetRequestExpectations(
    QuicSessionRequest* request) {
  if (!host_resolution_notified_) {
    request->ExpectOnHostResolution();
  }
  // Callers do not need to wait for OnQuicSessionCreationComplete if the
  // kAsyncQuicSession flag is not set because session creation will be fully
  // synchronous, so no need to call ExpectQuicSessionCreation.
  if (!base::FeatureList::IsEnabled(features::kAsyncQuicSession)) {
    return;
  }
  if (session_creation_notified_) {
    // The signal already fired. A new request must not be promised a second
    // one, not even while the other connector is still creating a session.
    return;
  }
  // Promise the session creation signal only while it can still fire. That
  // is before the host resolution signal fired, while an attempt of either
  // connector has not finished creating its session, or while a failed
  // session creation result is being held for later delivery.
  if (!host_resolution_notified_ ||
      (primary_connector_ && primary_connector_->AwaitingSessionCreation()) ||
      (secondary_connector_ &&
       secondary_connector_->AwaitingSessionCreation()) ||
      held_session_creation_result_.has_value()) {
    request->ExpectQuicSessionCreation();
  }
}

void QuicSessionPool::AsyncDnsJob::UpdatePriority(
    RequestPriority old_priority,
    RequestPriority new_priority) {
  if (old_priority == new_priority) {
    return;
  }

  if (service_endpoint_request_ && !resolution_finished_) {
    service_endpoint_request_->ChangeRequestPriority(new_priority);
  }
}

void QuicSessionPool::AsyncDnsJob::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  // An attempt that is still in flight describes the connection the requests
  // are waiting for, so it is preferred over the recorded failure. The
  // primary slot comes first because it holds the IPv6 side once both slots
  // are filled.
  for (const EndpointConnector* connector :
       {primary_connector_.get(), secondary_connector_.get()}) {
    if (connector && connector->has_attempt()) {
      connector->PopulateNetErrorDetails(details);
      return;
    }
  }
  if (last_attempt_failure_.has_value()) {
    details->connection_info = last_attempt_failure_->details.connection_info;
    details->quic_connection_error =
        last_attempt_failure_->details.quic_connection_error;
  }
}

void QuicSessionPool::AsyncDnsJob::OnServiceEndpointsUpdated() {
  usable_endpoints_.reset();
  if (!service_endpoint_request_->EndpointsCryptoReady()) {
    return;
  }

  std::optional<int> rv = ProcessServiceEndpointResults();
  if (!rv.has_value()) {
    // Nothing to attempt yet. Wait for the next update or the final result.
    return;
  }
  MaybeNotifyHostResolutionAndComplete(*rv);
}

void QuicSessionPool::AsyncDnsJob::OnServiceEndpointRequestFinished(int rv) {
  CHECK(!resolution_finished_);
  usable_endpoints_.reset();

  rv = DoResolveHostComplete(rv);
  MaybeNotifyHostResolutionAndComplete(rv);
}

void QuicSessionPool::AsyncDnsJob::MaybeNotifyHostResolutionAndComplete(
    int rv) {
  if (!host_resolution_notified_) {
    auto weak_this = weak_factory_.GetWeakPtr();
    NotifyRequestsOfHostResolution(rv);
    if (!weak_this) {
      return;
    }
  }
  if (rv != ERR_IO_PENDING) {
    // This destroys the job and its resolver request. The resolver supports
    // request destruction inside this delegate callback.
    CompleteJob(rv);
  }
}

void QuicSessionPool::AsyncDnsJob::CompleteJob(int rv) {
  slow_timer_.Stop();
  if (!session_creation_notified_ &&
      held_session_creation_result_.has_value()) {
    // The job is completing, so no later attempt can replace this result.
    // Deliver the held failure now.
    auto weak_this = weak_factory_.GetWeakPtr();
    NotifyRequestsOfSessionCreation(*held_session_creation_result_);
    if (!weak_this) {
      return;
    }
  }
  if (!callback_.is_null()) {
    std::move(callback_).Run(rv);
  }
}

void QuicSessionPool::AsyncDnsJob::NotifyRequestsOfHostResolution(int rv) {
  CHECK(!host_resolution_notified_);
  host_resolution_notified_ = true;

  // A notified request may reenter the pool and add or remove requests on
  // this job. Iterate over a snapshot of WeakPtrs, and skip requests that were
  // removed or destroyed. Requests added mid-notification never expected this
  // signal because `host_resolution_notified_` is already set.
  auto weak_this = weak_factory_.GetWeakPtr();
  std::vector<base::WeakPtr<QuicSessionRequest>> snapshot;
  snapshot.reserve(requests().size());
  for (QuicSessionRequest* request : requests()) {
    snapshot.push_back(request->GetWeakPtr());
  }
  for (const auto& weak_request : snapshot) {
    if (!weak_this) {
      return;
    }
    if (!weak_request) {
      continue;
    }
    if (!weak_this->requests().contains(weak_request.get())) {
      continue;
    }
    weak_request->OnHostResolutionComplete(rv, dns_resolution_start_time_,
                                           dns_resolution_end_time_);
  }
}

const std::vector<QuicSessionPool::AsyncDnsJob::UsableEndpoint>&
QuicSessionPool::AsyncDnsJob::GetUsableEndpoints() const {
  if (usable_endpoints_.has_value()) {
    return *usable_endpoints_;
  }

  std::vector<UsableEndpoint> usable_endpoints;
  base::span<const ServiceEndpoint> endpoints =
      service_endpoint_request_->GetEndpointResults();
  const bool svcb_optional = IsSvcbOptional(endpoints);
  for (const ServiceEndpoint& endpoint : endpoints) {
    quic::ParsedQuicVersion endpoint_quic_version = pool_->SelectQuicVersion(
        quic_version_, endpoint.metadata, svcb_optional);
    if (!endpoint_quic_version.IsKnown()) {
      continue;
    }
    if (endpoint.ipv4_endpoints.empty() && endpoint.ipv6_endpoints.empty()) {
      continue;
    }
    usable_endpoints.emplace_back(endpoint, endpoint_quic_version);
  }
  usable_endpoints_ = std::move(usable_endpoints);
  return *usable_endpoints_;
}

QuicSessionPool::AsyncDnsJob::AttemptParams
QuicSessionPool::AsyncDnsJob::GetAttemptParams() const {
  AttemptParams params;
  params.cert_verify_flags = cert_verify_flags_;
  params.dns_resolution_start_time = dns_resolution_start_time_;
  params.dns_resolution_end_time = dns_resolution_end_time_;
  params.resolution_details = service_endpoint_request_->GetResolutionDetails();
  params.retry_on_alternate_network_before_handshake =
      retry_on_alternate_network_before_handshake_;
  params.use_dns_aliases = use_dns_aliases_;
  if (use_dns_aliases_) {
    params.dns_aliases = service_endpoint_request_->GetDnsAliasResults();
  }
  params.session_creation_initiator = session_creation_initiator_;
  params.quic_session_establishment_reason = quic_session_establishment_reason_;
  params.connection_management_config = connection_management_config_;
  return params;
}

bool QuicSessionPool::AsyncDnsJob::MaybePoolToExistingSession() {
  const std::vector<UsableEndpoint>& usable_endpoints = GetUsableEndpoints();
  for (const UsableEndpoint& usable : usable_endpoints) {
    if (pool_->HasMatchingIpSessionForServiceEndpoint(
            key_, usable.endpoint,
            service_endpoint_request_->GetDnsAliasResults(), use_dns_aliases_,
            log_negative_ip_pool_result_)) {
      LogConnectionIpPooling(true);
      return true;
    }
  }

  // Record misses only for the first endpoints checked. Re-checks on later
  // results would inflate the recorded misses.
  if (!usable_endpoints.empty()) {
    log_negative_ip_pool_result_ = false;
  }

  return false;
}

std::optional<QuicSessionPool::AsyncDnsJob::Candidate>
QuicSessionPool::AsyncDnsJob::TakeNextCandidate(
    const EndpointConnector* connector) {
  // While the secondary slot is empty the primary connector may use both
  // families, and every visible IPv6 candidate ranks above any IPv4 one.
  // Once both slots are filled the primary takes IPv6 and the secondary
  // takes IPv4 while DNS resolution is in flight. Once DNS resolution
  // finishes, connectors may attempt the other family's remaining candidates
  // if their preferred family is exhausted.
  const bool slots_are_exclusive =
      secondary_connector_ != nullptr && !resolution_finished_;
  const bool takes_ipv6 = connector == primary_connector_.get();
  CHECK(takes_ipv6 || connector == secondary_connector_.get());

  const std::vector<UsableEndpoint>& usable_endpoints = GetUsableEndpoints();
  for (bool ipv6 : {takes_ipv6, !takes_ipv6}) {
    if (slots_are_exclusive && ipv6 != takes_ipv6) {
      continue;
    }
    // Endpoints and the IPs within them keep the resolver's order.
    for (const UsableEndpoint& usable : usable_endpoints) {
      const std::vector<IPEndPoint>& ip_endpoints =
          ipv6 ? usable.endpoint.ipv6_endpoints
               : usable.endpoint.ipv4_endpoints;
      for (const IPEndPoint& ip_endpoint : ip_endpoints) {
        Candidate candidate{ip_endpoint, usable.endpoint.metadata,
                            usable.quic_version};
        if (std::ranges::contains(claimed_candidates_, candidate)) {
          continue;
        }
        claimed_candidates_.push_back(candidate);
        return candidate;
      }
    }
  }
  return std::nullopt;
}

void QuicSessionPool::AsyncDnsJob::OnAttemptFailed(
    int rv,
    const NetErrorDetails& details) {
  last_attempt_failure_ = AttemptFailure{rv, details};
}

void QuicSessionPool::AsyncDnsJob::OnSessionCreationDecided(
    int rv,
    const EndpointConnector* connector) {
  CHECK(connector == primary_connector_.get() ||
        connector == secondary_connector_.get());
  if (session_creation_notified_) {
    // The other connector already decided the signal. Its result stands.
    return;
  }
  if (rv != OK && rv != ERR_IO_PENDING) {
    // A failure signal makes the waiting requests give up on QUIC, but this
    // job may still try another candidate. Hold the result until the job's
    // outcome is known.
    held_session_creation_result_ = rv;
    return;
  }
  NotifyRequestsOfSessionCreation(rv);
}

void QuicSessionPool::AsyncDnsJob::NotifyRequestsOfSessionCreation(int rv) {
  CHECK(!session_creation_notified_);
  session_creation_notified_ = true;
  held_session_creation_result_.reset();

  // A notified request may reenter the pool and add or remove requests on
  // this job. Iterate over a snapshot of WeakPtrs, and skip requests that were
  // removed or destroyed. Requests added mid-notification are not promised
  // this signal because the connector's attempt has already finished creating
  // its session.
  auto weak_this = weak_factory_.GetWeakPtr();
  std::vector<base::WeakPtr<QuicSessionRequest>> snapshot;
  snapshot.reserve(requests().size());
  for (QuicSessionRequest* request : requests()) {
    snapshot.push_back(request->GetWeakPtr());
  }
  for (const auto& weak_request : snapshot) {
    if (!weak_this) {
      return;
    }
    if (!weak_request) {
      continue;
    }
    if (!weak_this->requests().contains(weak_request.get())) {
      continue;
    }
    weak_request->OnQuicSessionCreationComplete(rv);
  }
}

void QuicSessionPool::AsyncDnsJob::OnConnectorComplete(
    int rv,
    EndpointConnector* connector) {
  CHECK_NE(rv, ERR_IO_PENDING);
  CHECK(connector == primary_connector_.get() ||
        connector == secondary_connector_.get());

  if (rv == OK) {
    // The first connector to succeed settles the job. The other one and its
    // in-flight attempt are destroyed here.
    DestroyOtherConnector(connector);
    CompleteJob(OK);
    return;
  }

  // This connector ran out of candidates. Keep the job going while the other
  // connector still runs an attempt, and while DNS can still deliver more
  // candidates.
  const EndpointConnector* other = OtherConnector(connector);
  if (!resolution_finished_ || (other && other->has_attempt())) {
    return;
  }
  CompleteJob(LastFailureResult().value_or(rv));
}

void QuicSessionPool::AsyncDnsJob::DestroyOtherConnector(
    const EndpointConnector* connector) {
  if (connector == primary_connector_.get()) {
    secondary_connector_.reset();
    return;
  }
  CHECK_EQ(connector, secondary_connector_.get());
  // The connector that succeeded moves into the primary slot. The assignment
  // destroys the connector that was there, together with its in-flight
  // attempt.
  primary_connector_ = std::move(secondary_connector_);
}

void QuicSessionPool::AsyncDnsJob::MaybeStartSlowTimer() {
  if (slow_timer_started_ || secondary_connector_ || !primary_connector_ ||
      !primary_connector_->has_attempt()) {
    return;
  }
  const base::TimeDelta delay = features::kAsyncDnsQuicJobSlowTimerDelay.Get();
  if (!delay.is_positive()) {
    // Two attempts at once are disabled. The primary connector walks the
    // candidates by itself.
    return;
  }
  slow_timer_started_ = true;
  slow_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&AsyncDnsJob::OnSlowTimer, base::Unretained(this)));
}

void QuicSessionPool::AsyncDnsJob::OnSlowTimer() {
  CHECK(primary_connector_);
  CHECK(!secondary_connector_);

  secondary_connector_ = std::make_unique<EndpointConnector>(this);
  if (!primary_connector_->is_attempting_ipv6()) {
    // The connector in the primary slot is not on IPv6, either because it
    // attempts IPv4 or because it waits for a candidate. The slots decide the
    // families from now on and the IPv6 side has to be the primary one, so
    // move the connectors into the other slot.
    std::swap(primary_connector_, secondary_connector_);
  }

  std::optional<int> rv = AdvanceConnectors();
  if (rv.has_value() && *rv != ERR_IO_PENDING) {
    // A connector settled the job while it started its attempt.
    CompleteJob(*rv);
  }
}

bool QuicSessionPool::AsyncDnsJob::HasWaitingConnector() const {
  return (primary_connector_ && primary_connector_->is_waiting_on_dns()) ||
         (secondary_connector_ && secondary_connector_->is_waiting_on_dns());
}

bool QuicSessionPool::AsyncDnsJob::HasAttemptInFlight() const {
  return (primary_connector_ && primary_connector_->has_attempt()) ||
         (secondary_connector_ && secondary_connector_->has_attempt());
}

const QuicSessionPool::EndpointConnector*
QuicSessionPool::AsyncDnsJob::OtherConnector(
    const EndpointConnector* connector) const {
  if (connector == primary_connector_.get()) {
    return secondary_connector_.get();
  }
  return primary_connector_.get();
}

std::optional<int> QuicSessionPool::AsyncDnsJob::LastFailureResult() const {
  if (!last_attempt_failure_.has_value()) {
    return std::nullopt;
  }
  return last_attempt_failure_->rv;
}

int QuicSessionPool::AsyncDnsJob::DoResolveHost() {
  dns_resolution_start_time_ = base::TimeTicks::Now();

  HostResolver::ResolveHostParameters parameters;
  parameters.initial_priority = priority_;
  parameters.secure_dns_policy = key_.session_key().secure_dns_policy();
  service_endpoint_request_ = host_resolver_->CreateServiceEndpointRequest(
      HostResolver::Host(key_.destination()),
      key_.session_key().network_anonymization_key(),
      key_.session_key().target_network(), net_log_, std::move(parameters));
  return service_endpoint_request_->Start(this);
}

int QuicSessionPool::AsyncDnsJob::DoResolveHostComplete(int rv) {
  resolution_finished_ = true;
  MaybeSetDnsResolutionEndTime();

  // A resolver error fails the job only while no attempt has run. Once a
  // connector exists the attempts decide the outcome.
  if (rv != OK && !primary_connector_) {
    return rv;
  }

  return ProcessServiceEndpointResults().value_or(
      ERR_DNS_NO_MATCHING_SUPPORTED_ALPN);
}

std::optional<int>
QuicSessionPool::AsyncDnsJob::ProcessServiceEndpointResults() {
  // Nothing to do while every connector keeps an attempt in flight. A
  // connector reads the new results when it advances, and re-checks IP
  // pooling before its next attempt.
  if (primary_connector_ && !HasWaitingConnector()) {
    return ERR_IO_PENDING;
  }

  // If another request pooled to an existing session and activated the key
  // while we were waiting for async DNS resolution, this job will be
  // redundant. The active session is already in the pool.
  if (pool_->HasActiveSession(key_.session_key())) {
    MaybeSetDnsResolutionEndTime();
    return OK;
  }

  if (MaybePoolToExistingSession()) {
    MaybeSetDnsResolutionEndTime();
    return OK;
  }

  if (GetUsableEndpoints().empty()) {
    // Nothing is visible to attempt. An attempt that is still in flight can
    // settle the job by itself.
    if (HasAttemptInFlight()) {
      return ERR_IO_PENDING;
    }
    // If an attempt already failed and DNS has finished, no more candidates
    // can arrive, so report that failure.
    if (resolution_finished_) {
      return LastFailureResult();
    }
    return std::nullopt;
  }

  // Attempts read the DNS end time, so stamp it before advancing the
  // connectors.
  MaybeSetDnsResolutionEndTime();

  if (!primary_connector_) {
    primary_connector_ = std::make_unique<EndpointConnector>(this);
  }

  std::optional<int> result = AdvanceConnectors();
  if (result.has_value()) {
    return result;
  }

  // Every visible candidate was already attempted. Fail only when DNS has
  // finished. Until then more candidates may arrive.
  if (!resolution_finished_) {
    return std::nullopt;
  }
  return LastFailureResult();
}

std::optional<int> QuicSessionPool::AsyncDnsJob::AdvanceConnector(
    EndpointConnector* connector) {
  if (!connector) {
    return std::nullopt;
  }
  if (!connector->is_waiting_on_dns()) {
    // The connector keeps the attempt it already has in flight.
    return ERR_IO_PENDING;
  }
  return connector->TryAdvance();
}

std::optional<int> QuicSessionPool::AsyncDnsJob::AdvanceConnectors() {
  // The primary slot advances first so that it claims candidates first.
  const std::optional<int> primary_rv =
      AdvanceConnector(primary_connector_.get());
  if (primary_rv == OK) {
    DestroyOtherConnector(primary_connector_.get());
    return OK;
  }

  // The primary connector never drops the secondary one, so the secondary
  // slot still holds what it held above.
  const std::optional<int> secondary_rv =
      AdvanceConnector(secondary_connector_.get());
  if (secondary_rv == OK) {
    DestroyOtherConnector(secondary_connector_.get());
    return OK;
  }

  MaybeStartSlowTimer();

  if (primary_rv == ERR_IO_PENDING || secondary_rv == ERR_IO_PENDING) {
    return ERR_IO_PENDING;
  }
  return std::nullopt;
}

void QuicSessionPool::AsyncDnsJob::MaybeSetDnsResolutionEndTime() {
  if (dns_resolution_end_time_.is_null()) {
    dns_resolution_end_time_ = base::TimeTicks::Now();
  }
}

bool QuicSessionPool::AsyncDnsJob::IsSvcbOptional(
    base::span<const ServiceEndpoint> endpoints) const {
  // If SVCB/HTTPS resolution succeeded, the client supports ECH, and all
  // alternative endpoints support ECH, disable the A/AAAA fallback. See
  // Section 5.1 of draft-ietf-tls-svcb-ech-08.
  if (!pool_->ssl_config_service_->GetSSLContextConfig().ech_enabled ||
      pool_->ssl_config_service_->GetEchMode(key().session_key().host()) ==
          EchMode::kDisabled) {
    return true;  // ECH is not supported for this request.
  }

  return !HostResolver::AllAlternativeEndpointsHaveEch(endpoints);
}

}  // namespace net

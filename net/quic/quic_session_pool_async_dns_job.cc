// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_pool_async_dns_job.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
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
  }
  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  }
  return rv > 0 ? OK : rv;
}

void QuicSessionPool::AsyncDnsJob::SetRequestExpectations(
    QuicSessionRequest* request) {
  if (!host_resolution_finished_) {
    request->ExpectOnHostResolution();
  }
  // Callers do not need to wait for OnQuicSessionCreationComplete if the
  // kAsyncQuicSession flag is not set because session creation will be fully
  // synchronous, so no need to call ExpectQuicSessionCreation.
  if (!base::FeatureList::IsEnabled(features::kAsyncQuicSession)) {
    return;
  }
  // Promise the session creation signal only while it can still fire:
  // before resolution settles the job, or while the connector's attempt has
  // not finished creating its session.
  if (!host_resolution_finished_ ||
      (connector_ && connector_->AwaitingSessionCreation())) {
    request->ExpectQuicSessionCreation();
  }
}

void QuicSessionPool::AsyncDnsJob::UpdatePriority(
    RequestPriority old_priority,
    RequestPriority new_priority) {
  if (old_priority == new_priority) {
    return;
  }

  if (service_endpoint_request_ && !host_resolution_finished_) {
    service_endpoint_request_->ChangeRequestPriority(new_priority);
  }
}

void QuicSessionPool::AsyncDnsJob::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  if (connector_) {
    connector_->PopulateNetErrorDetails(details);
  }
}

void QuicSessionPool::AsyncDnsJob::OnServiceEndpointsUpdated() {
  usable_endpoints_.reset();
  // TODO(crbug.com/531975349): Act on partial results instead of waiting
  // for the final result.
}

void QuicSessionPool::AsyncDnsJob::OnServiceEndpointRequestFinished(int rv) {
  CHECK(!host_resolution_finished_);
  usable_endpoints_.reset();
  rv = DoResolveHostComplete(rv);

  // A notified request may reenter the pool and add or remove requests on
  // this job. Iterate over a snapshot of WeakPtrs, and skip requests that were
  // removed or destroyed. Requests added mid-notification never expected this
  // signal because `host_resolution_finished_` is already set.
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

  if (weak_this && rv != ERR_IO_PENDING && !callback_.is_null()) {
    std::move(callback_).Run(rv);
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
  for (const UsableEndpoint& usable : GetUsableEndpoints()) {
    if (pool_->HasMatchingIpSessionForServiceEndpoint(
            key_, usable.endpoint,
            service_endpoint_request_->GetDnsAliasResults(),
            use_dns_aliases_)) {
      LogConnectionIpPooling(true);
      return true;
    }
  }
  return false;
}

void QuicSessionPool::AsyncDnsJob::OnSessionCreationDecided(int rv) {
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

void QuicSessionPool::AsyncDnsJob::OnConnectorComplete(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  if (!callback_.is_null()) {
    std::move(callback_).Run(rv);
  }
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
  host_resolution_finished_ = true;
  dns_resolution_end_time_ = base::TimeTicks::Now();
  if (rv != OK) {
    return rv;
  }

  // If another request pooled to an existing session and activated the key
  // while we were waiting for async DNS resolution, this job will be
  // redundant. The active session is already in the pool.
  if (pool_->HasActiveSession(key_.session_key())) {
    return OK;
  }

  if (MaybePoolToExistingSession()) {
    return OK;
  }

  if (GetUsableEndpoints().empty()) {
    return ERR_DNS_NO_MATCHING_SUPPORTED_ALPN;
  }

  connector_ = std::make_unique<EndpointConnector>(this);
  std::optional<int> result = connector_->OnEndpointsUpdated();
  // The final results contain a usable endpoint, so the connector either
  // settles synchronously or starts an attempt.
  CHECK(result.has_value());
  return *result;
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

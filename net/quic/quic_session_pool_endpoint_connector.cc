// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_pool_endpoint_connector.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/quic/quic_session_pool_async_dns_job.h"

namespace net {

QuicSessionPool::EndpointConnector::EndpointConnector(AsyncDnsJob* job)
    : job_(job) {}

QuicSessionPool::EndpointConnector::~EndpointConnector() {
  if (attempt_in_flight_) {
    attempt_->Cancel();
  }
}

std::optional<int> QuicSessionPool::EndpointConnector::OnEndpointsUpdated() {
  // TODO(crbug.com/531975349): Support fallback and racing over multiple
  // candidate endpoints. For now this makes a single attempt.
  if (attempt_) {
    return ERR_IO_PENDING;
  }

  const std::vector<AsyncDnsJob::UsableEndpoint>& usable_endpoints =
      job_->GetUsableEndpoints();
  if (usable_endpoints.empty()) {
    return std::nullopt;
  }

  // Attempt the first usable endpoint, preferring IPv6 within the endpoint.
  const AsyncDnsJob::UsableEndpoint& usable = usable_endpoints.front();
  const IPEndPoint& ip_endpoint = usable.endpoint.ipv6_endpoints.empty()
                                      ? usable.endpoint.ipv4_endpoints.front()
                                      : usable.endpoint.ipv6_endpoints.front();

  AsyncDnsJob::AttemptParams params = job_->GetAttemptParams();
  // Passing a null `crypto_client_config_handle` is safe because the owning
  // job holds a handle for as long as this attempt can be alive.
  attempt_ = std::make_unique<QuicSessionAttempt>(
      this, ip_endpoint, usable.endpoint.metadata, usable.quic_version,
      params.cert_verify_flags, params.dns_resolution_start_time,
      params.dns_resolution_end_time, std::move(params.resolution_details),
      params.retry_on_alternate_network_before_handshake,
      params.use_dns_aliases, std::move(params.dns_aliases),
      /*crypto_client_config_handle=*/nullptr,
      params.session_creation_initiator,
      params.quic_session_establishment_reason,
      params.connection_management_config);

  int rv = attempt_->Start(base::BindOnce(&EndpointConnector::OnAttemptComplete,
                                          weak_factory_.GetWeakPtr()));
  attempt_in_flight_ = (rv == ERR_IO_PENDING);
  return rv;
}

bool QuicSessionPool::EndpointConnector::AwaitingSessionCreation() const {
  return attempt_ && !attempt_->session_creation_finished();
}

void QuicSessionPool::EndpointConnector::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  if (attempt_) {
    attempt_->PopulateNetErrorDetails(details);
  }
}

QuicSessionPool* QuicSessionPool::EndpointConnector::GetQuicSessionPool() {
  return job_->pool();
}

const QuicSessionAliasKey& QuicSessionPool::EndpointConnector::GetKey() {
  return job_->key();
}

const NetLogWithSource& QuicSessionPool::EndpointConnector::GetNetLog() {
  return job_->net_log();
}

void QuicSessionPool::EndpointConnector::OnConnectionFailedOnDefaultNetwork() {
  job_->OnConnectionFailedOnDefaultNetwork();
}

void QuicSessionPool::EndpointConnector::OnQuicSessionCreationComplete(int rv) {
  job_->OnSessionCreationDecided(rv);
}

void QuicSessionPool::EndpointConnector::OnAttemptComplete(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  attempt_in_flight_ = false;
  job_->OnConnectorComplete(rv);
}

}  // namespace net

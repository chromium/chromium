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

std::optional<int> QuicSessionPool::EndpointConnector::TryAdvance() {
  if (attempt_) {
    return ERR_IO_PENDING;
  }

  // Another job may have created a usable session while the previous
  // attempt ran. Pool to it instead of attempting again.
  if (last_attempt_error_.has_value() && job_->MaybePoolToExistingSession()) {
    return OK;
  }

  while (true) {
    std::optional<AsyncDnsJob::Candidate> candidate = ClaimNextCandidate();
    if (!candidate.has_value()) {
      return std::nullopt;
    }

    AsyncDnsJob::AttemptParams params = job_->GetAttemptParams();
    // Passing a null `crypto_client_config_handle` is safe because the owning
    // job holds a handle for as long as this attempt can be alive.
    attempt_ = std::make_unique<QuicSessionAttempt>(
        this, candidate->ip_endpoint, std::move(candidate->metadata),
        candidate->quic_version, params.cert_verify_flags,
        params.dns_resolution_start_time, params.dns_resolution_end_time,
        std::move(params.resolution_details),
        params.retry_on_alternate_network_before_handshake,
        params.use_dns_aliases, std::move(params.dns_aliases),
        /*crypto_client_config_handle=*/nullptr,
        params.session_creation_initiator,
        params.quic_session_establishment_reason,
        params.connection_management_config);

    int rv = attempt_->Start(base::BindOnce(
        &EndpointConnector::OnAttemptComplete, weak_factory_.GetWeakPtr()));
    attempt_in_flight_ = (rv == ERR_IO_PENDING);
    if (rv == OK || rv == ERR_IO_PENDING) {
      return rv;
    }
    // The attempt failed while starting. Continue with the next candidate.
    RecordAttemptFailure(rv);
  }
}

bool QuicSessionPool::EndpointConnector::AwaitingSessionCreation() const {
  return attempt_ && !attempt_->session_creation_finished();
}

void QuicSessionPool::EndpointConnector::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  if (attempt_) {
    attempt_->PopulateNetErrorDetails(details);
    return;
  }
  if (!last_attempt_error_.has_value()) {
    return;
  }
  details->connection_info = last_attempt_details_.connection_info;
  details->quic_connection_error = last_attempt_details_.quic_connection_error;
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

std::optional<QuicSessionPool::AsyncDnsJob::Candidate>
QuicSessionPool::EndpointConnector::ClaimNextCandidate() {
  const std::vector<AsyncDnsJob::UsableEndpoint>& usable_endpoints =
      job_->GetUsableEndpoints();
  // Endpoints and the IPs within them keep the resolver's order.
  for (bool ipv6 : {true, false}) {
    for (const AsyncDnsJob::UsableEndpoint& usable : usable_endpoints) {
      const std::vector<IPEndPoint>& ip_endpoints =
          ipv6 ? usable.endpoint.ipv6_endpoints
               : usable.endpoint.ipv4_endpoints;
      for (const IPEndPoint& ip_endpoint : ip_endpoints) {
        AsyncDnsJob::Candidate candidate{ip_endpoint, usable.endpoint.metadata,
                                         usable.quic_version};
        if (!job_->ClaimCandidate(candidate)) {
          continue;
        }
        return candidate;
      }
    }
  }
  return std::nullopt;
}

void QuicSessionPool::EndpointConnector::RecordAttemptFailure(int rv) {
  CHECK(attempt_);
  last_attempt_error_ = rv;
  last_attempt_details_ = NetErrorDetails();
  attempt_->PopulateNetErrorDetails(&last_attempt_details_);
  attempt_.reset();
}

void QuicSessionPool::EndpointConnector::OnAttemptComplete(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  attempt_in_flight_ = false;
  if (rv != OK) {
    RecordAttemptFailure(rv);
    std::optional<int> result = TryAdvance();
    if (result == ERR_IO_PENDING) {
      return;
    }
    // Nothing else could be attempted. Report the most recent failure so
    // the job can fail or keep waiting for more DNS results.
    rv = result.value_or(*last_attempt_error_);
  }
  job_->OnConnectorComplete(rv);
}

}  // namespace net

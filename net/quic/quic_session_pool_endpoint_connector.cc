// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_pool_endpoint_connector.h"

#include <string_view>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "net/base/address_family.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_event_type.h"
#include "net/quic/quic_session_pool_async_dns_job.h"

namespace net {

QuicSessionPool::EndpointConnector::EndpointConnector(
    AsyncDnsJob* job,
    const char* name,
    bool created_by_slow_timer)
    : job_(job), name_(name), created_by_slow_timer_(created_by_slow_timer) {}

bool QuicSessionPool::EndpointConnector::is_stale() const {
  return job_->IsStaleConnector(*this);
}

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
    std::optional<AsyncDnsJob::Candidate> candidate =
        job_->TakeNextCandidate(*this);
    if (!candidate.has_value()) {
      return std::nullopt;
    }

    AsyncDnsJob::AttemptParams params = job_->GetAttemptParams();
    attempt_start_time_ = base::TimeTicks::Now();
    ++attempts_started_;
    attempt_id_ =
        job_->OnAttemptStarted(*this, *candidate, attempt_start_time_);
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
        params.session_creation_initiator, params.quic_connection_reuse_details,
        params.connection_management_config, is_stale());

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

bool QuicSessionPool::EndpointConnector::is_attempting_ipv6() const {
  return attempt_ && attempt_->ip_endpoint().GetFamily() == ADDRESS_FAMILY_IPV6;
}

std::optional<IPEndPoint>
QuicSessionPool::EndpointConnector::attempt_ip_endpoint() const {
  if (!attempt_) {
    return std::nullopt;
  }
  return attempt_->ip_endpoint();
}

bool QuicSessionPool::EndpointConnector::AwaitingSessionCreation() const {
  return attempt_ && !attempt_->session_creation_finished();
}

void QuicSessionPool::EndpointConnector::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  CHECK(attempt_);
  attempt_->PopulateNetErrorDetails(details);
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
  job_->OnSessionCreationDecided(rv, *this);
}

void QuicSessionPool::EndpointConnector::RecordAttemptFailure(int rv) {
  CHECK(attempt_);
  CHECK(attempt_id_.has_value());
  last_attempt_error_ = rv;
  NetErrorDetails details;
  attempt_->PopulateNetErrorDetails(&details);

  job_->net_log().AddEvent(
      NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_ATTEMPT_FAILED, [&] {
        return base::DictValue()
            .Set("attempt_id", *attempt_id_)
            .Set("connector", name_)
            .Set("slot", job_->SlotName(*this))
            .Set("ip_endpoint", attempt_->ip_endpoint().ToString())
            .Set("net_error", rv);
      });
  attempt_.reset();
  attempt_id_.reset();
  job_->OnAttemptFailed(rv, details);
}

void QuicSessionPool::EndpointConnector::OnAttemptComplete(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  attempt_in_flight_ = false;
  if (rv != OK) {
    RecordAttemptFailure(rv);
    if (rv != ERR_ABORTED) {
      std::optional<int> result = TryAdvance();
      if (result == ERR_IO_PENDING) {
        return;
      }
      // Nothing else could be attempted. Report the most recent failure so
      // the job can fail or keep waiting for more DNS results.
      rv = result.value_or(*last_attempt_error_);
    }
  }
  job_->OnConnectorComplete(rv, *this);
}

}  // namespace net

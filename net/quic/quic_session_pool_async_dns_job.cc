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
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "net/base/address_family.h"
#include "net/base/ech_mode.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_event_type.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/quic/quic_session_pool_endpoint_connector.h"
#include "net/ssl/ssl_config_service.h"
#include "url/url_constants.h"

namespace net {

QuicSessionPool::AsyncDnsJob::ConnectionState::ConnectionState() = default;
QuicSessionPool::AsyncDnsJob::ConnectionState::~ConnectionState() = default;

QuicSessionPool::AsyncDnsJob::ConnectionState&
QuicSessionPool::AsyncDnsJob::GetState(const EndpointConnector& connector) {
  if (&connector == stale_state_.primary_connector.get() ||
      &connector == stale_state_.secondary_connector.get()) {
    return stale_state_;
  }
  return fresh_state_;
}

const QuicSessionPool::AsyncDnsJob::ConnectionState&
QuicSessionPool::AsyncDnsJob::GetState(
    const EndpointConnector& connector) const {
  if (&connector == stale_state_.primary_connector.get() ||
      &connector == stale_state_.secondary_connector.get()) {
    return stale_state_;
  }
  return fresh_state_;
}

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
    QuicConnectionReuseDetails quic_connection_reuse_details,
    std::optional<ConnectionManagementConfig> connection_management_config,
    const NetLogWithSource& net_log)
    : Job(pool,
          std::move(key),
          std::move(client_config_handle),
          priority,
          session_creation_initiator,
          quic_connection_reuse_details,
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
      connection_management_config_(connection_management_config) {
  CHECK_EQ(quic_version_.IsKnown(), !require_dns_https_alpn_);
}

QuicSessionPool::AsyncDnsJob::~AsyncDnsJob() = default;

int QuicSessionPool::AsyncDnsJob::Run(CompletionOnceCallback callback) {
  int rv = DoResolveHost();
  if (rv != ERR_IO_PENDING) {
    LogServiceEndpointRequestFinished(rv);
    rv = DoResolveHostComplete(rv);
    // Resolution completed synchronously, before any request attached. The
    // host resolution signal has no receiver and must not be promised.
    host_resolution_notified_ = true;
  }
  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  } else {
    // The job settled without completing through CompleteJob().
    RecordMetrics(rv);
    LogJobComplete(rv);
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
      (fresh_state_.primary_connector &&
       fresh_state_.primary_connector->AwaitingSessionCreation()) ||
      (fresh_state_.secondary_connector &&
       fresh_state_.secondary_connector->AwaitingSessionCreation()) ||
      (stale_state_.primary_connector &&
       stale_state_.primary_connector->AwaitingSessionCreation()) ||
      (stale_state_.secondary_connector &&
       stale_state_.secondary_connector->AwaitingSessionCreation()) ||
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
       {fresh_state_.primary_connector.get(),
        fresh_state_.secondary_connector.get(),
        stale_state_.primary_connector.get(),
        stale_state_.secondary_connector.get()}) {
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

  const bool endpoints_crypto_ready =
      service_endpoint_request_->EndpointsCryptoReady();
  net_log_.AddEvent(
      NetLogEventType::
          QUIC_SESSION_POOL_ASYNC_DNS_JOB_SERVICE_ENDPOINTS_UPDATED,
      [&] {
        base::DictValue dict;
        dict.Set("endpoints_crypto_ready", endpoints_crypto_ready);
        dict.Set("endpoint_count",
                 static_cast<int>(
                     service_endpoint_request_->GetEndpointResults().size()));
        if (endpoints_crypto_ready) {
          dict.Set("usable_endpoint_count",
                   static_cast<int>(GetUsableEndpoints().size()));
        }
        return dict;
      });

  if (!endpoints_crypto_ready) {
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

  LogServiceEndpointRequestFinished(rv);

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
  RecordMetrics(rv);
  LogJobComplete(rv);
  fresh_state_.slow_timer.Stop();
  stale_state_.slow_timer.Stop();
  if (!session_creation_notified_) {
    if (rv == OK) {
      auto weak_this = weak_factory_.GetWeakPtr();
      NotifyRequestsOfSessionCreation(OK);
      if (!weak_this) {
        return;
      }
    } else if (held_session_creation_result_.has_value()) {
      // The job is completing, so no later attempt can replace this result.
      // Deliver the held failure now.
      auto weak_this = weak_factory_.GetWeakPtr();
      NotifyRequestsOfSessionCreation(*held_session_creation_result_);
      if (!weak_this) {
        return;
      }
    }
  }
  if (!callback_.is_null()) {
    std::move(callback_).Run(rv);
  }
}

void QuicSessionPool::AsyncDnsJob::NotifyRequestsOfHostResolution(int rv) {
  CHECK(!host_resolution_notified_);
  host_resolution_notified_ = true;

  net_log_.AddEventWithIntParams(
      NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_HOST_RESOLUTION_SIGNALED,
      "net_error", rv);

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
  params.quic_connection_reuse_details = quic_connection_reuse_details_;
  params.connection_management_config = connection_management_config_;
  return params;
}

bool QuicSessionPool::AsyncDnsJob::MaybePoolToExistingSession() {
  if (service_endpoint_request_) {
    if (service_endpoint_request_->IsStaleWhileRefreshing()) {
      stale_endpoints_evaluated_for_pooling_ = true;
    } else if (stale_endpoints_evaluated_for_pooling_) {
      stale_endpoints_evaluated_for_pooling_ = false;
      num_endpoints_evaluated_for_pooling_ = 0;
    }
  }

  const std::vector<UsableEndpoint>& usable_endpoints = GetUsableEndpoints();
  for (size_t i = num_endpoints_evaluated_for_pooling_;
       i < usable_endpoints.size(); ++i) {
    const UsableEndpoint& usable = usable_endpoints[i];
    if (QuicChromiumClientSession* session =
            pool_->HasMatchingIpSessionForServiceEndpoint(
                key_, usable.endpoint,
                service_endpoint_request_->GetDnsAliasResults(),
                use_dns_aliases_, log_negative_ip_pool_result_)) {
      LogConnectionIpPooling(true);
      success_source_ = SuccessSource::kIpPooling;
      net_log_.AddEventReferencingSource(
          NetLogEventType::QUIC_SESSION_POOL_JOB_RESULT,
          session->net_log().source());
      return true;
    }
  }
  num_endpoints_evaluated_for_pooling_ = usable_endpoints.size();

  // Record misses only for the first endpoints checked. Re-checks on later
  // results would inflate the recorded misses.
  if (!usable_endpoints.empty()) {
    log_negative_ip_pool_result_ = false;
  }

  return false;
}

std::optional<QuicSessionPool::AsyncDnsJob::Candidate>
QuicSessionPool::AsyncDnsJob::TakeNextCandidate(
    const EndpointConnector& connector) {
  ConnectionState& state = GetState(connector);
  if (&state == &stale_state_ &&
      !service_endpoint_request_->IsStaleWhileRefreshing()) {
    return std::nullopt;
  }

  // While the secondary slot is empty the primary connector may use both
  // families, and every visible IPv6 candidate ranks above any IPv4 one.
  // Once both slots are filled the primary takes IPv6 and the secondary
  // takes IPv4 while DNS resolution is in flight. Once DNS resolution
  // finishes, connectors may attempt the other family's remaining candidates
  // if their preferred family is exhausted.
  const bool slots_are_exclusive =
      state.secondary_connector != nullptr && !resolution_finished_;
  const bool takes_ipv6 = &connector == state.primary_connector.get();
  CHECK(takes_ipv6 || &connector == state.secondary_connector.get());

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
        if (std::ranges::contains(state.claimed_candidates_, candidate)) {
          continue;
        }
        state.claimed_candidates_.push_back(candidate);
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
    const EndpointConnector& connector) {
  ConnectionState& state = GetState(connector);
  CHECK(&connector == state.primary_connector.get() ||
        &connector == state.secondary_connector.get());
  if (session_creation_notified_) {
    // The other connector already decided the signal. Its result stands.
    return;
  }
  if (rv != OK && rv != ERR_IO_PENDING) {
    if (features::kAsyncDnsQuicJobFastFail.Get()) {
      NotifyRequestsOfSessionCreation(rv);
      return;
    }
    // A failure signal makes the waiting requests give up on QUIC, but this
    // job may still try another candidate. Hold the result until the job's
    // outcome is known.
    held_session_creation_result_ = rv;
    net_log_.AddEventWithIntParams(
        NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_SESSION_CREATION_HELD,
        "net_error", rv);
    return;
  }
  NotifyRequestsOfSessionCreation(rv);
}

void QuicSessionPool::AsyncDnsJob::NotifyRequestsOfSessionCreation(int rv) {
  CHECK(!session_creation_notified_);
  session_creation_notified_ = true;
  held_session_creation_result_.reset();

  net_log_.AddEventWithIntParams(
      NetLogEventType::
          QUIC_SESSION_POOL_ASYNC_DNS_JOB_SESSION_CREATION_SIGNALED,
      "net_error", rv);

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
    EndpointConnector& connector) {
  ConnectionState& state = GetState(connector);
  CHECK_NE(rv, ERR_IO_PENDING);
  CHECK(&connector == state.primary_connector.get() ||
        &connector == state.secondary_connector.get());

  if (rv == OK) {
    // The first connector to settle successfully wins. The other one and its
    // in-flight attempt are destroyed here.
    DestroyOtherConnector(connector);
    CompleteJob(OK);
    return;
  }

  // This connector ran out of candidates. Keep the job going while any other
  // connector still runs an attempt, and while DNS can still deliver more
  // candidates.
  if (!resolution_finished_ || HasAttemptInFlight()) {
    return;
  }
  CompleteJob(LastFailureResult().value_or(rv));
}

// Connectors are named "first" and "second" upon instantiation, distinct from
// their slot names ("primary" and "secondary"). Since connectors can swap
// slots (e.g. in OnSlowTimer), instance names are kept separate to accurately
// trace each connector's lifetime in NetLog events.
const char* QuicSessionPool::AsyncDnsJob::SlotName(
    const EndpointConnector& connector) const {
  const ConnectionState& state = GetState(connector);
  CHECK(&connector == state.primary_connector.get() ||
        &connector == state.secondary_connector.get());
  return &connector == state.primary_connector.get() ? "primary" : "secondary";
}

int QuicSessionPool::AsyncDnsJob::OnAttemptStarted(
    const EndpointConnector& connector,
    const Candidate& candidate,
    base::TimeTicks start_time) {
  ++attempt_count_;
  const int attempt_id = static_cast<int>(attempt_count_);
  if (first_attempt_start_time_.is_null()) {
    first_attempt_start_time_ = start_time;
  }
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_ATTEMPT_STARTED, [&] {
        return base::DictValue()
            .Set("attempt_id", attempt_id)
            .Set("connector", connector.name())
            .Set("ip_endpoint", candidate.ip_endpoint.ToString())
            .Set("address_family", AddressFamilyToString(GetAddressFamily(
                                       candidate.ip_endpoint.address())))
            .Set("slot", SlotName(connector))
            .Set("quic_version",
                 quic::ParsedQuicVersionToString(candidate.quic_version))
            .Set("metadata", candidate.metadata.ToValue())
            .Set("resolution_in_flight", !resolution_finished_)
            .Set("is_stale", connector.is_stale());
      });
  MaybeStartSlowTimer(GetState(connector));
  return attempt_id;
}

void QuicSessionPool::AsyncDnsJob::LogJobComplete(int rv) const {
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_COMPLETE, [&] {
        const char* completion_reason = "failed";
        if (rv == OK) {
          CHECK(success_source_ != SuccessSource::kNone);
          switch (success_source_) {
            case SuccessSource::kNone:
              break;
            case SuccessSource::kInitialConnectorFirstAttempt:
            case SuccessSource::kInitialConnectorLaterAttempt:
            case SuccessSource::kSlowTimerConnector:
              completion_reason = "attempt_succeeded";
              break;
            case SuccessSource::kActiveSession:
              completion_reason = "active_session";
              break;
            case SuccessSource::kIpPooling:
              completion_reason = "ip_pooling";
              break;
          }
        }
        return base::DictValue()
            .Set("net_error", rv)
            .Set("attempt_count", static_cast<int>(attempt_count_))
            .Set("completion_reason", completion_reason);
      });
}

void QuicSessionPool::AsyncDnsJob::LogServiceEndpointRequestFinished(
    int rv) const {
  net_log_.AddEvent(
      NetLogEventType::
          QUIC_SESSION_POOL_ASYNC_DNS_JOB_SERVICE_ENDPOINT_REQUEST_FINISHED,
      [&] {
        const bool fresh_attempt_started =
            (fresh_state_.primary_connector &&
             fresh_state_.primary_connector->attempts_started() > 0) ||
            (fresh_state_.secondary_connector &&
             fresh_state_.secondary_connector->attempts_started() > 0);
        return base::DictValue()
            .Set("net_error", rv)
            .Set("ignored_late_error", rv != OK && fresh_attempt_started);
      });
}

void QuicSessionPool::AsyncDnsJob::RecordMetrics(int rv) const {
  if (rv != OK) {
    base::UmaHistogramCounts100(
        "Net.QuicSession.AsyncDnsJob.AttemptsPerJob.JobFailed", attempt_count_);
    // Time from the first connection attempt until the job failed. Jobs that
    // fail before starting an attempt are not recorded.
    if (!first_attempt_start_time_.is_null()) {
      base::UmaHistogramMediumTimes(
          "Net.QuicSession.AsyncDnsJob.TimeToFailure",
          base::TimeTicks::Now() - first_attempt_start_time_);
    }
    return;
  }

  base::UmaHistogramCounts100(
      "Net.QuicSession.AsyncDnsJob.AttemptsPerJob.JobSucceeded",
      attempt_count_);
  CHECK(success_source_ != SuccessSource::kNone);
  base::UmaHistogramEnumeration("Net.QuicSession.AsyncDnsJob.SuccessSource",
                                success_source_);
  if (successful_attempt_start_time_.is_null()) {
    return;
  }
  if (resolution_finished_time_.is_null()) {
    // DNS is canceled when the job succeeds, so this is a lower bound on the
    // time from attempt start to the final DNS result.
    base::UmaHistogramMediumTimes(
        "Net.QuicSession.AsyncDnsJob.SuccessfulAttemptElapsedTime."
        "JobSuccessWithDnsInFlight",
        base::TimeTicks::Now() - successful_attempt_start_time_);
  } else {
    // The attempt may start after DNS finishes. Record zero in that case.
    base::UmaHistogramMediumTimes(
        "Net.QuicSession.AsyncDnsJob.SuccessfulAttemptElapsedTime."
        "FinalDnsResult",
        std::max(base::TimeDelta(),
                 resolution_finished_time_ - successful_attempt_start_time_));
  }
}

void QuicSessionPool::AsyncDnsJob::DestroyOtherConnector(
    const EndpointConnector& connector) {
  if (!connector.has_attempt()) {
    // The connector succeeded by pooling, without an attempt.
    success_source_ = SuccessSource::kIpPooling;
  } else if (connector.created_by_slow_timer()) {
    success_source_ = SuccessSource::kSlowTimerConnector;
  } else if (connector.attempts_started() > 1) {
    success_source_ = SuccessSource::kInitialConnectorLaterAttempt;
  } else {
    success_source_ = SuccessSource::kInitialConnectorFirstAttempt;
  }
  if (connector.has_attempt()) {
    successful_attempt_start_time_ = connector.attempt_start_time();
  }
  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_CONNECTOR_SETTLED_JOB,
      [&] {
        base::DictValue dict;
        dict.Set("connector", connector.name());
        dict.Set("slot", SlotName(connector));
        const std::optional<IPEndPoint> ip_endpoint =
            connector.attempt_ip_endpoint();
        if (ip_endpoint.has_value()) {
          CHECK(connector.attempt_id().has_value());
          dict.Set("attempt_id", *connector.attempt_id());
          dict.Set("ip_endpoint", ip_endpoint->ToString());
        }
        dict.Set("completion_reason",
                 connector.has_attempt() ? "attempt_succeeded" : "ip_pooling");
        base::ListValue canceled_attempts;
        for (const EndpointConnector* other :
             {fresh_state_.primary_connector.get(),
              fresh_state_.secondary_connector.get(),
              stale_state_.primary_connector.get(),
              stale_state_.secondary_connector.get()}) {
          if (other && other != &connector && other->has_attempt()) {
            CHECK(other->attempt_id().has_value());
            if (!dict.contains("canceled_attempt_id")) {
              dict.Set("canceled_attempt_id", *other->attempt_id());
              dict.Set("canceled_ip_endpoint",
                       other->attempt_ip_endpoint()->ToString());
            }
            base::DictValue canceled_dict;
            canceled_dict.Set("attempt_id", *other->attempt_id());
            canceled_dict.Set("ip_endpoint",
                              other->attempt_ip_endpoint()->ToString());
            canceled_attempts.Append(std::move(canceled_dict));
          }
        }
        if (!canceled_attempts.empty()) {
          dict.Set("canceled_attempts", std::move(canceled_attempts));
        }
        return dict;
      });
  ConnectionState& state = GetState(connector);
  ConnectionState& other_state =
      (&state == &fresh_state_) ? stale_state_ : fresh_state_;

  // Abort the opposing race entirely (i.e. fresh vs stale) to ensure no
  // background tasks or socket attempts outlive the job resolution.
  other_state.slow_timer.Stop();
  other_state.primary_connector.reset();
  other_state.secondary_connector.reset();

  // Cleanup the losing connector in the winning state.
  if (&connector == state.primary_connector.get()) {
    state.secondary_connector.reset();
    return;
  }
  CHECK_EQ(&connector, state.secondary_connector.get());
  // The connector that succeeded moves into the primary slot. The assignment
  // destroys the connector that was there, together with its in-flight
  // attempt.
  state.primary_connector = std::move(state.secondary_connector);
}

void QuicSessionPool::AsyncDnsJob::MaybeStartSlowTimer(ConnectionState& state) {
  if (state.slow_timer_started || state.secondary_connector ||
      !state.primary_connector || !state.primary_connector->has_attempt()) {
    return;
  }
  base::TimeDelta delay = features::kQuicSlowTimerDelay.Get();

  if (base::FeatureList::IsEnabled(features::kQuicSlowTimerBasedOnRTT)) {
    std::optional<base::TimeDelta> rtt =
        pool()->GetSmoothedRtt(key_.session_key().server_id(),
                               key_.session_key().network_anonymization_key(),
                               key_.session_key().proxy_chain());

    if (rtt.has_value()) {
      base::TimeDelta min_delay = features::kQuicSlowTimerMin.Get();
      base::TimeDelta max_delay = features::kQuicSlowTimerMax.Get();
      if (min_delay > max_delay) {
        std::swap(min_delay, max_delay);
      }
      delay =
          std::clamp(rtt.value() * features::kQuicSlowTimerRTTMultiplier.Get(),
                     min_delay, max_delay);
    }
  }

  if (!delay.is_positive()) {
    // Two attempts at once are disabled. The primary connector walks the
    // candidates by itself.
    return;
  }
  base::TimeTicks start_time = state.primary_connector->attempt_start_time();
  if (!start_time.is_null()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
    delay = std::max(base::TimeDelta(), delay - elapsed);
  }
  state.slow_timer_started = true;
  state.slow_timer.Start(FROM_HERE, delay,
                         base::BindOnce(&AsyncDnsJob::OnSlowTimer,
                                        base::Unretained(this), &state));
  net_log_.AddEventWithIntParams(
      NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_SLOW_TIMER_ARMED,
      "delay_ms", static_cast<int>(delay.InMilliseconds()));
}

void QuicSessionPool::AsyncDnsJob::OnSlowTimer(ConnectionState* state) {
  CHECK(state->primary_connector);
  CHECK(!state->secondary_connector);

  net_log_.AddEvent(
      NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_SLOW_TIMER_FIRED);

  state->secondary_connector = std::make_unique<EndpointConnector>(
      this, "second", /*created_by_slow_timer=*/true);
  if (!state->primary_connector->is_attempting_ipv6()) {
    // The connector in the primary slot is not on IPv6, either because it
    // attempts IPv4 or because it waits for a candidate. The slots decide the
    // families from now on and the IPv6 side has to be the primary one, so
    // move the connectors into the other slot.
    std::swap(state->primary_connector, state->secondary_connector);
    net_log_.AddEvent(
        NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_SLOTS_SWAPPED);
  }

  std::optional<int> rv = AdvanceConnectors(*state);
  if (rv.has_value() && *rv != ERR_IO_PENDING) {
    // A connector settled the job while it advanced.
    CompleteJob(*rv);
  }
}

bool QuicSessionPool::AsyncDnsJob::HasWaitingConnector(
    const ConnectionState& state) const {
  return (state.primary_connector &&
          state.primary_connector->is_waiting_on_dns()) ||
         (state.secondary_connector &&
          state.secondary_connector->is_waiting_on_dns());
}

bool QuicSessionPool::AsyncDnsJob::HasAttemptInFlight() const {
  return (fresh_state_.primary_connector &&
          fresh_state_.primary_connector->has_attempt()) ||
         (fresh_state_.secondary_connector &&
          fresh_state_.secondary_connector->has_attempt()) ||
         (stale_state_.primary_connector &&
          stale_state_.primary_connector->has_attempt()) ||
         (stale_state_.secondary_connector &&
          stale_state_.secondary_connector->has_attempt());
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
  if (base::FeatureList::IsEnabled(features::kOptimisticDnsForQuic)) {
    parameters.cache_usage = HostResolver::ResolveHostParameters::CacheUsage::
        STALE_ALLOWED_WHILE_REFRESHING;
  }
  service_endpoint_request_ = host_resolver_->CreateServiceEndpointRequest(
      HostResolver::Host(key_.destination()),
      key_.session_key().network_anonymization_key(),
      key_.session_key().target_network(), net_log_, std::move(parameters));
  return service_endpoint_request_->Start(this);
}

int QuicSessionPool::AsyncDnsJob::DoResolveHostComplete(int rv) {
  resolution_finished_ = true;
  resolution_finished_time_ = base::TimeTicks::Now();
  MaybeSetDnsResolutionEndTime();

  // A resolver error fails the job immediately unless a fresh attempt is
  // somehow already running. Stale attempts are speculative and should not
  // override a hard DNS error, so we abort them here (consistent with
  // TcpConnectJob).
  const bool fresh_attempt_started =
      (fresh_state_.primary_connector &&
       fresh_state_.primary_connector->attempts_started() > 0) ||
      (fresh_state_.secondary_connector &&
       fresh_state_.secondary_connector->attempts_started() > 0);
  if (rv != OK && !fresh_attempt_started) {
    stale_state_.slow_timer.Stop();
    stale_state_.primary_connector.reset();
    stale_state_.secondary_connector.reset();
    return rv;
  }

  return ProcessServiceEndpointResults().value_or(
      rv != OK ? rv : ERR_DNS_NO_MATCHING_SUPPORTED_ALPN);
}

void QuicSessionPool::AsyncDnsJob::MaybePromoteStaleConnectors() {
  if (!service_endpoint_request_ ||
      service_endpoint_request_->IsStaleWhileRefreshing()) {
    return;
  }

  stale_state_.slow_timer.Stop();

  // Stale connectors whose in-flight attempts target endpoints present in fresh
  // DNS results are promoted to the fresh state machine. If an in-flight stale
  // attempt targets an endpoint absent from fresh DNS, it is intentionally not
  // promoted, but is allowed to complete its current attempt in the background
  // (consistent with TcpConnectJob behavior).
  auto try_promote = [&](std::unique_ptr<EndpointConnector>& stale_connector) {
    if (stale_connector && stale_connector->has_attempt_in_flight()) {
      const std::optional<IPEndPoint> address =
          stale_connector->attempt_ip_endpoint();
      if (address && IsEndpointInFreshList(*address)) {
        if (!fresh_state_.primary_connector ||
            !fresh_state_.primary_connector->has_attempt_in_flight()) {
          fresh_state_.primary_connector = std::move(stale_connector);
        } else if (!fresh_state_.secondary_connector ||
                   !fresh_state_.secondary_connector->has_attempt_in_flight()) {
          fresh_state_.secondary_connector = std::move(stale_connector);
        }

        // Ensure the fresh state knows it has already tried this endpoint,
        // whether promotion succeeded or was blocked by busy fresh slots.
        for (const UsableEndpoint& usable : GetUsableEndpoints()) {
          if (std::ranges::contains(usable.endpoint.ipv4_endpoints, *address) ||
              std::ranges::contains(usable.endpoint.ipv6_endpoints, *address)) {
            Candidate fresh_candidate{*address, usable.endpoint.metadata,
                                      usable.quic_version};
            if (!std::ranges::contains(fresh_state_.claimed_candidates_,
                                       fresh_candidate)) {
              fresh_state_.claimed_candidates_.push_back(fresh_candidate);
            }
          }
        }
      }
    }
  };

  try_promote(stale_state_.primary_connector);
  try_promote(stale_state_.secondary_connector);

  if (fresh_state_.primary_connector && fresh_state_.secondary_connector) {
    if (!fresh_state_.primary_connector->is_attempting_ipv6() &&
        fresh_state_.secondary_connector->is_attempting_ipv6()) {
      std::swap(fresh_state_.primary_connector,
                fresh_state_.secondary_connector);
      net_log_.AddEvent(
          NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_SLOTS_SWAPPED);
    }
  }

  // If a secondary connector is now present in the fresh state (e.g. promoted
  // from stale), cancel any pending slow timer to prevent OnSlowTimer from
  // asserting CHECK(!state->secondary_connector).
  if (fresh_state_.secondary_connector) {
    fresh_state_.slow_timer.Stop();
  }
}

bool QuicSessionPool::AsyncDnsJob::IsStaleConnector(
    const EndpointConnector& connector) const {
  return &GetState(connector) == &stale_state_;
}

bool QuicSessionPool::AsyncDnsJob::IsEndpointInFreshList(
    const IPEndPoint& endpoint) const {
  for (const UsableEndpoint& usable : GetUsableEndpoints()) {
    if (std::ranges::contains(usable.endpoint.ipv4_endpoints, endpoint) ||
        std::ranges::contains(usable.endpoint.ipv6_endpoints, endpoint)) {
      return true;
    }
  }
  return false;
}

std::optional<int>
QuicSessionPool::AsyncDnsJob::ProcessServiceEndpointResults() {
  MaybePromoteStaleConnectors();

  ConnectionState& state = service_endpoint_request_->IsStaleWhileRefreshing()
                               ? stale_state_
                               : fresh_state_;

  // If another request pooled to an existing session and activated the key
  // while we were waiting for async DNS resolution, this job will be
  // redundant. The active session is already in the pool.
  if (pool_->HasActiveSession(key_.session_key())) {
    success_source_ = SuccessSource::kActiveSession;
    net_log_.AddEvent(NetLogEventType::QUIC_SESSION_POOL_JOB_RESULT, [&] {
      QuicChromiumClientSession* session =
          pool_->FindExistingSession(key_.session_key(), key_.destination());
      CHECK(session);
      base::DictValue dict;
      session->net_log().source().AddToEventParameters(dict);
      return dict;
    });
    MaybeSetDnsResolutionEndTime();
    return OK;
  }

  if (MaybePoolToExistingSession()) {
    MaybeSetDnsResolutionEndTime();
    return OK;
  }

  // We already checked for eager pooling matches. Since none were found,
  // there is nothing to do while every connector keeps an attempt in flight.
  // A connector will read the new results if/when it advances.
  if (state.primary_connector && !HasWaitingConnector(state)) {
    MaybeStartSlowTimer(state);
    return ERR_IO_PENDING;
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

  if (!state.primary_connector) {
    state.primary_connector = std::make_unique<EndpointConnector>(
        this, "first", /*created_by_slow_timer=*/false);
  }

  std::optional<int> result = AdvanceConnectors(state);
  if (result.has_value()) {
    return result;
  }

  // Every visible candidate was already attempted. Fail only when DNS has
  // finished. Until then more candidates may arrive.
  if (!resolution_finished_) {
    return std::nullopt;
  }
  if (HasAttemptInFlight()) {
    return ERR_IO_PENDING;
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

std::optional<int> QuicSessionPool::AsyncDnsJob::AdvanceConnectors(
    ConnectionState& state) {
  // The primary slot advances first so that it claims candidates first.
  const std::optional<int> primary_rv =
      AdvanceConnector(state.primary_connector.get());
  if (primary_rv == OK) {
    DestroyOtherConnector(*state.primary_connector);
    return OK;
  }

  // The primary connector never drops the secondary one, so the secondary
  // slot still holds what it held above.
  const std::optional<int> secondary_rv =
      AdvanceConnector(state.secondary_connector.get());
  if (secondary_rv == OK) {
    DestroyOtherConnector(*state.secondary_connector);
    return OK;
  }

  MaybeStartSlowTimer(state);

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
  if (pool_->ssl_config_service_->GetEchMode(key().session_key().host()) ==
      EchMode::kDisabled) {
    return true;  // ECH is not supported for this request.
  }

  return !HostResolver::AllAlternativeEndpointsHaveEch(endpoints);
}

}  // namespace net

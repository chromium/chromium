// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_attempt_manager.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_id_helper.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/tracing.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool_attempt_manager_quic_attempt.h"
#include "net/http/http_stream_pool_attempt_manager_tcp_based_attempt.h"
#include "net/http/http_stream_pool_group.h"
#include "net/http/http_stream_pool_handle.h"
#include "net/http/http_stream_pool_job.h"
#include "net/log/net_log_util.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_pool.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_attempt.h"
#include "net/socket/stream_socket_close_reason.h"
#include "net/socket/stream_socket_handle.h"
#include "net/socket/tcp_stream_attempt.h"
#include "net/socket/tls_stream_attempt.h"
#include "net/spdy/multiplexed_session_creation_initiator.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

namespace {

StreamSocketHandle::SocketReuseType GetReuseTypeFromIdleStreamSocket(
    const StreamSocket& stream_socket) {
  return stream_socket.WasEverUsed()
             ? StreamSocketHandle::SocketReuseType::kReusedIdle
             : StreamSocketHandle::SocketReuseType::kUnusedIdle;
}

base::Value::Dict GetServiceEndpointRequestAsValue(
    HostResolver::ServiceEndpointRequest* request) {
  base::Value::Dict dict;
  base::Value::List endpoints;
  for (const auto& endpoint : request->GetEndpointResults()) {
    endpoints.Append(endpoint.ToValue());
  }
  dict.Set("endpoints", std::move(endpoints));
  dict.Set("endpoints_crypto_ready", request->EndpointsCryptoReady());
  return dict;
}

}  // namespace

// static
std::string_view HttpStreamPool::AttemptManager::CanAttemptResultToString(
    CanAttemptResult result) {
  switch (result) {
    case CanAttemptResult::kAttempt:
      return "Attempt";
    case CanAttemptResult::kReachedPoolLimit:
      return "ReachedPoolLimit";
    case CanAttemptResult::kNoPendingJob:
      return "NoPendingJob";
    case CanAttemptResult::kBlockedTcpBasedAttempt:
      return "BlockedTcpBasedAttempt";
    case CanAttemptResult::kThrottledForSpdy:
      return "ThrottledForSpdy";
    case CanAttemptResult::kReachedGroupLimit:
      return "ReachedGroupLimit";
  }
}

// static
std::string_view HttpStreamPool::AttemptManager::TcpBasedAttemptStateToString(
    TcpBasedAttemptState state) {
  switch (state) {
    case TcpBasedAttemptState::kNotStarted:
      return "NotStarted";
    case TcpBasedAttemptState::kAttempting:
      return "Attempting";
    case TcpBasedAttemptState::kSucceededAtLeastOnce:
      return "SucceededAtLeastOnce";
    case TcpBasedAttemptState::kAllEndpointsFailed:
      return "AllEndpointsFailed";
  }
}

// static
std::string_view HttpStreamPool::AttemptManager::InitialAttemptStateToString(
    InitialAttemptState state) {
  switch (state) {
    case InitialAttemptState::kOther:
      return "Other";
    case InitialAttemptState::kCanUseQuicWithKnownVersion:
      return "CanUseQuicWithKnownVersion";
    case InitialAttemptState::kCanUseQuicWithKnownVersionAndSupportsSpdy:
      return "CanUseQuicWithKnownVersionAndSupportsSpdy";
    case InitialAttemptState::kCanUseQuicWithUnknownVersion:
      return "CanUseQuicWithUnknownVersion";
    case InitialAttemptState::kCanUseQuicWithUnknownVersionAndSupportsSpdy:
      return "CanUseQuicWithUnknownVersionAndSupportsSpdy";
    case InitialAttemptState::kCannotUseQuicWithKnownVersion:
      return "CannotUseQuicWithKnownVersion";
    case InitialAttemptState::kCannotUseQuicWithKnownVersionAndSupportsSpdy:
      return "CannotUseQuicWithKnownVersionAndSupportsSpdy";
    case InitialAttemptState::kCannotUseQuicWithUnknownVersion:
      return "CannotUseQuicWithUnknownVersion";
    case InitialAttemptState::kCannotUseQuicWithUnknownVersionAndSupportsSpdy:
      return "CannotUseQuicWithUnknownVersionAndSupportsSpdy";
  }
}

HttpStreamPool::AttemptManager::AttemptManager(Group* group, NetLog* net_log)
    : group_(group),
      net_log_(NetLogWithSource::Make(
          net_log,
          NetLogSourceType::HTTP_STREAM_POOL_ATTEMPT_MANAGER)),
      track_(base::trace_event::GetNextGlobalTraceId()),
      created_time_(base::TimeTicks::Now()),
      request_jobs_(NUM_PRIORITIES),
      tcp_based_attempt_delay_(GetTcpBasedAttemptDelay()),
      should_block_tcp_based_attempt_(!tcp_based_attempt_delay_.is_zero()) {
  CHECK(group_);

  TRACE_EVENT_BEGIN("net.stream", "AttemptManager::AttemptManager", track_,
                    "destination", stream_key().destination().Serialize());

  net_log_.BeginEvent(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_ALIVE, [&] {
        base::Value::Dict dict;
        dict.Set("stream_key", stream_key().ToValue());
        dict.Set("tcp_based_attempt_delay",
                 static_cast<int>(tcp_based_attempt_delay_.InMilliseconds()));
        dict.Set("should_block_tcp_based_attempt",
                 should_block_tcp_based_attempt_);
        group_->net_log().source().AddToEventParameters(dict);
        return dict;
      });
  group_->net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_GROUP_ATTEMPT_MANAGER_CREATED,
      net_log_.source());
  base::UmaHistogramTimes("Net.HttpStreamPool.TcpBasedAttemptDelay",
                          tcp_based_attempt_delay_);

  if (UsingTls()) {
    SSLConfig ssl_config;
    ssl_config.privacy_mode = stream_key().privacy_mode();
    ssl_config.disable_cert_verification_network_fetches =
        stream_key().disable_cert_network_fetches();

    ssl_config.alpn_protos = http_network_session()->GetAlpnProtos();
    ssl_config.application_settings =
        http_network_session()->GetApplicationSettings();
    http_network_session()->http_server_properties()->MaybeForceHTTP11(
        stream_key().destination(), stream_key().network_anonymization_key(),
        &ssl_config);

    ssl_config.ignore_certificate_errors =
        http_network_session()->params().ignore_certificate_errors;
    ssl_config.network_anonymization_key =
        stream_key().network_anonymization_key();

    base_ssl_config_.emplace(std::move(ssl_config));
  }
}

HttpStreamPool::AttemptManager::~AttemptManager() {
  base::UmaHistogramTimes("Net.HttpStreamPool.AttemptManagerAliveTime",
                          base::TimeTicks::Now() - created_time_);
  net_log().EndEvent(NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_ALIVE);
  group_->net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_GROUP_ATTEMPT_MANAGER_DESTROYED,
      net_log_.source());
  TRACE_EVENT_END("net.stream", track_);
}

void HttpStreamPool::AttemptManager::RequestStream(Job* job) {
  CHECK(availability_state_ == AvailabilityState::kAvailable);

  TRACE_EVENT_INSTANT("net.stream", "AttemptManager::StartJob", track_,
                      NetLogWithSourceToFlow(job->request_net_log()));

  net_log_.AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_START_JOB, [&] {
        base::Value::Dict dict;
        dict.Set("priority", job->priority());
        base::Value::List allowed_bad_certs_list;
        for (const auto& cert_and_status : job->allowed_bad_certs()) {
          allowed_bad_certs_list.Append(
              cert_and_status.cert->subject().GetDisplayName());
        }
        dict.Set("allowed_bad_certs", std::move(allowed_bad_certs_list));
        dict.Set("enable_ip_based_pooling", job->enable_ip_based_pooling());
        dict.Set("enable_alternative_services",
                 job->enable_alternative_services());
        dict.Set("quic_version",
                 quic::ParsedQuicVersionToString(job->quic_version()));
        job->net_log().source().AddToEventParameters(dict);
        return dict;
      });
  job->request_net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_JOB_BOUND,
      net_log_.source());
  job->net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_JOB_BOUND,
      net_log_.source());

  if (job->respect_limits() == RespectLimits::kIgnore) {
    limit_ignoring_jobs_.emplace(job);
  }

  if (!job->enable_ip_based_pooling()) {
    ip_based_pooling_disabling_jobs_.emplace(job);
  }

  if (!job->enable_alternative_services()) {
    alternative_service_disabling_jobs_.emplace(job);
  }

  // HttpStreamPool should check the existing QUIC/SPDY sessions before calling
  // this method.
  DCHECK(!CanUseExistingQuicSession());
  DCHECK(!HasAvailableSpdySession());

  request_jobs_.Insert(job, job->priority());

  MaybeChangeServiceEndpointRequestPriority();

  // Check idle streams. If found, notify the job that an HttpStream is ready.
  std::unique_ptr<StreamSocket> stream_socket = group_->GetIdleStreamSocket();
  if (stream_socket) {
    CHECK(!group_->force_quic());
    const StreamSocketHandle::SocketReuseType reuse_type =
        GetReuseTypeFromIdleStreamSocket(*stream_socket);
    // It's important to create an HttpBasicStream synchronously because we
    // already took the ownership of the idle stream socket. If we don't create
    // an HttpBasicStream here, another call of this method might exceed the
    // per-group limit.
    CreateTextBasedStreamAndNotify(std::move(stream_socket), reuse_type,
                                   LoadTimingInfo::ConnectTiming());
    return;
  }

  if (base_ssl_config_.has_value()) {
    base_ssl_config_->allowed_bad_certs = job->allowed_bad_certs();
  }
  quic_version_ = job->quic_version();

  StartInternal(job);

  return;
}

void HttpStreamPool::AttemptManager::Preconnect(Job* job) {
  CHECK(availability_state_ == AvailabilityState::kAvailable);

  TRACE_EVENT_INSTANT("net.stream", "AttemptManager::Preconnect", track_,
                      NetLogWithSourceToFlow(job->request_net_log()));

  // If `job` is resumed, there could be enough streams at this point.
  if (group_->ActiveStreamSocketCount() >= job->num_streams()) {
    NotifyJobOfPreconnectComplete(job, OK);
    return;
  }

  net_log_.AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_PRECONNECT, [&] {
        base::Value::Dict dict;
        dict.Set("num_streams", static_cast<int>(job->num_streams()));
        dict.Set("quic_version",
                 quic::ParsedQuicVersionToString(job->quic_version()));
        job->delegate_net_log().source().AddToEventParameters(dict);
        return dict;
      });
  job->delegate_net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_JOB_CONTROLLER_PRECONNECT_BOUND,
      net_log_.source());

  // HttpStreamPool should check the existing QUIC/SPDY sessions before calling
  // this method.
  DCHECK(!CanUseExistingQuicSession());
  DCHECK(!HasAvailableSpdySession());

  preconnect_jobs_.emplace(job);
  quic_version_ = job->quic_version();

  StartInternal(job);
}

void HttpStreamPool::AttemptManager::OnServiceEndpointsUpdated() {
  TRACE_EVENT_INSTANT("net.stream", "AttemptManager::OnServiceEndpointsUpdated",
                      track_);
  net_log().AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_DNS_RESOLUTION_UPDATED,
      [&] {
        return GetServiceEndpointRequestAsValue(
            service_endpoint_request_.get());
      });

  ProcessServiceEndpointChanges();
}

void HttpStreamPool::AttemptManager::OnServiceEndpointRequestFinished(int rv) {
  TRACE_EVENT_INSTANT("net.stream",
                      "AttemptManager::OnServiceEndpointRequestFinished",
                      track_, "result", rv);
  CHECK(!service_endpoint_request_finished_);
  CHECK(service_endpoint_request_);

  service_endpoint_request_finished_ = true;
  dns_resolution_end_time_ = base::TimeTicks::Now();
  resolve_error_info_ = service_endpoint_request_->GetResolveErrorInfo();

  net_log().AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_DNS_RESOLUTION_FINISHED,
      [&] {
        base::Value::Dict dict =
            GetServiceEndpointRequestAsValue(service_endpoint_request_.get());
        dict.Set("result", ErrorToString(rv));
        dict.Set("resolve_error", resolve_error_info_.error);
        return dict;
      });

  if (rv != OK) {
    // If service endpoint resolution failed, record an empty endpoint and the
    // result.
    connection_attempts_.emplace_back(IPEndPoint(), rv);
    HandleFinalError(rv);
    return;
  }

  CHECK(!service_endpoint_request_->GetEndpointResults().empty());
  ProcessServiceEndpointChanges();
}

HostResolver::ServiceEndpointRequest*
HttpStreamPool::AttemptManager::GetServiceEndpointRequest() {
  return service_endpoint_request_.get();
}

bool HttpStreamPool::AttemptManager::IsSvcbOptional() {
  CHECK(service_endpoint_request_);
  CHECK(pool()->stream_attempt_params()->ssl_client_context);

  // Optional when the destination is not a SVCB-capable or ECH is disabled.
  if (!UsingTls() || !IsEchEnabled()) {
    return true;
  }

  base::span<const ServiceEndpoint> endpoints =
      service_endpoint_request_->GetEndpointResults();
  return !HostResolver::AllProtocolEndpointsHaveEch(endpoints);
}

bool HttpStreamPool::AttemptManager::HasEnoughTcpBasedAttemptsForSlowIPEndPoint(
    const IPEndPoint& ip_endpoint) {
  // TODO(crbug.com/383824591): Consider modifying the value of
  // IPEndPointStateMap to track the number of in-flight attempts per
  // IPEndPoint, if this loop is a bottlenek.
  size_t num_attempts = std::ranges::count(
      tcp_based_attempts_, ip_endpoint,
      [](const auto& entry) { return entry->attempt()->ip_endpoint(); });

  return num_attempts >=
         std::max(request_jobs_.size(), CalculateMaxPreconnectCount());
}

bool HttpStreamPool::AttemptManager::IsEndpointUsableForTcpBasedAttempt(
    const ServiceEndpoint& endpoint,
    bool svcb_optional) {
  // No ALPNs means that the endpoint is an authority A/AAAA endpoint, even if
  // we are still in the middle of DNS resolution.
  if (endpoint.metadata.supported_protocol_alpns.empty()) {
    return svcb_optional;
  }

  // See https://www.rfc-editor.org/rfc/rfc9460.html#section-9.3. Endpoints are
  // usable if there is an overlap between the endpoint's ALPNs and the
  // configured ones.
  return std::ranges::any_of(
      endpoint.metadata.supported_protocol_alpns, [&](const auto& alpn) {
        return base::Contains(http_network_session()->GetAlpnProtos(),
                              NextProtoFromString(alpn));
      });
}

HttpStreamPool::AttemptManager::InitialAttemptState
HttpStreamPool::AttemptManager::CalculateInitialAttemptState() {
  using enum InitialAttemptState;
  bool supports_spdy = SupportsSpdy();
  if (CanUseQuic()) {
    if (quic_version_.IsKnown()) {
      if (supports_spdy) {
        return kCanUseQuicWithKnownVersionAndSupportsSpdy;
      } else {
        return kCanUseQuicWithKnownVersion;
      }
    } else {
      if (supports_spdy) {
        return kCanUseQuicWithUnknownVersionAndSupportsSpdy;
      } else {
        return kCanUseQuicWithUnknownVersion;
      }
    }
  } else {
    if (quic_version_.IsKnown()) {
      if (supports_spdy) {
        return kCannotUseQuicWithKnownVersionAndSupportsSpdy;
      } else {
        return kCannotUseQuicWithKnownVersion;
      }
    } else {
      if (supports_spdy) {
        return kCannotUseQuicWithUnknownVersionAndSupportsSpdy;
      } else {
        return kCannotUseQuicWithUnknownVersion;
      }
    }
  }
}

void HttpStreamPool::AttemptManager::SetInitialAttemptState() {
  CHECK(!initial_attempt_state_.has_value());
  initial_attempt_state_ = CalculateInitialAttemptState();
  net_log_.AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_INITIAL_ATTEMPT_STATE,
      [&] {
        return base::Value::Dict().Set(
            "state", InitialAttemptStateToString(*initial_attempt_state_));
      });
  base::UmaHistogramEnumeration("Net.HttpStreamPool.InitialAttemptState2",
                                *initial_attempt_state_);
}

base::expected<SSLConfig, TlsStreamAttempt::GetSSLConfigError>
HttpStreamPool::AttemptManager::GetSSLConfig(const IPEndPoint& ip_endpoint) {
  CHECK(service_endpoint_request_);
  CHECK(service_endpoint_request_->EndpointsCryptoReady());

  SSLConfig ssl_config = *base_ssl_config_;
  ssl_config.early_data_enabled =
      http_network_session()->params().enable_early_data;

  if (!IsEchEnabled()) {
    return ssl_config;
  }

  const bool svcb_optional = IsSvcbOptional();
  for (auto& endpoint : service_endpoint_request_->GetEndpointResults()) {
    if (!IsEndpointUsableForTcpBasedAttempt(endpoint, svcb_optional)) {
      continue;
    }
    const std::vector<IPEndPoint>& ip_endpoints = ip_endpoint.address().IsIPv4()
                                                      ? endpoint.ipv4_endpoints
                                                      : endpoint.ipv6_endpoints;
    if (base::Contains(ip_endpoints, ip_endpoint)) {
      ssl_config.ech_config_list = endpoint.metadata.ech_config_list;
      return ssl_config;
    }
  }

  return base::unexpected(TlsStreamAttempt::GetSSLConfigError::kAbort);
}

void HttpStreamPool::AttemptManager::ProcessPendingJob() {
  if (is_shutting_down()) {
    return;
  }

  // Try to assign an idle stream to a job.
  if (request_jobs_.size() > 0) {
    std::unique_ptr<StreamSocket> stream_socket = group_->GetIdleStreamSocket();
    if (stream_socket) {
      const StreamSocketHandle::SocketReuseType reuse_type =
          GetReuseTypeFromIdleStreamSocket(*stream_socket);
      CreateTextBasedStreamAndNotify(std::move(stream_socket), reuse_type,
                                     LoadTimingInfo::ConnectTiming());
      return;
    }
  }

  const size_t pending_job_count = PendingRequestJobCount();
  const size_t pending_preconnect_count = PendingPreconnectCount();

  if (pending_job_count == 0 && pending_preconnect_count == 0) {
    return;
  }

  DCHECK(!HasAvailableSpdySession());

  MaybeAttemptTcpBased(/*exclude_ip_endpoint=*/std::nullopt,
                       /*max_attempts=*/1);
}

void HttpStreamPool::AttemptManager::CancelTcpBasedAttempts(
    StreamSocketCloseReason reason) {
  if (tcp_based_attempts_.empty()) {
    return;
  }

  const size_t num_cancel_attempts = tcp_based_attempts_.size();
  const size_t num_total_connecting_before =
      pool()->TotalConnectingStreamCount();
  while (!tcp_based_attempts_.empty()) {
    std::unique_ptr<TcpBasedAttempt> attempt =
        ExtractTcpBasedAttempt(tcp_based_attempts_.begin()->get());
    attempt->SetCancelReason(reason);
    attempt.reset();
  }
  CHECK_EQ(pool()->TotalConnectingStreamCount(),
           num_total_connecting_before - num_cancel_attempts);
  CHECK_EQ(slow_tcp_based_attempt_count_, 0u);

  base::UmaHistogramCounts100(
      base::StrCat({"Net.HttpStreamPool.TcpBasedAttemptCancelCount.",
                    StreamSocketCloseReasonToString(reason)}),
      num_cancel_attempts);

  ip_endpoint_state_tracker_.RemoveSlowAttemptingEndpoint();

  // If possible, try to complete asynchronously to avoid accessing deleted
  // `this` and `group_`. `this` and/or `group_` can be accessed after leaving
  // this method. Also, HttpStreamPool::OnSSLConfigChanged() calls this method
  // when walking through all groups. If we destroy `this` here, we will break
  // the loop.
  MaybeCompleteLater();
}

void HttpStreamPool::AttemptManager::OnJobComplete(Job* job) {
  preconnect_jobs_.erase(job);
  ip_based_pooling_disabling_jobs_.erase(job);
  alternative_service_disabling_jobs_.erase(job);

  auto notified_it = notified_jobs_.find(job);
  if (notified_it != notified_jobs_.end()) {
    notified_jobs_.erase(notified_it);
  } else {
    for (JobQueue::Pointer pointer = request_jobs_.FirstMax();
         !pointer.is_null();
         pointer = request_jobs_.GetNextTowardsLastMin(pointer)) {
      if (pointer.value() == job) {
        RemoveJobFromQueue(pointer);
        break;
      }
    }
  }

  MaybeCompleteLater();
}

void HttpStreamPool::AttemptManager::CancelJobs(int error) {
  HandleFinalError(error);
}

void HttpStreamPool::AttemptManager::CancelQuicAttempt(int error) {
  if (quic_attempt_) {
    quic_attempt_result_ = error;
    quic_attempt_.reset();
  }
}

size_t HttpStreamPool::AttemptManager::PendingRequestJobCount() const {
  return PendingCountInternal(request_jobs_.size());
}

size_t HttpStreamPool::AttemptManager::PendingPreconnectCount() const {
  size_t num_streams = CalculateMaxPreconnectCount();
  // Pending preconnect count is treated as zero when the maximum preconnect
  // socket count is less than or equal to the active stream socket count.
  // This behavior is for compatibility with the non-HEv3 code path. See
  // TransportClientSocketPool::RequestSockets().
  if (num_streams <= group_->ActiveStreamSocketCount()) {
    return 0;
  }
  return PendingCountInternal(num_streams);
}

const HttpStreamKey& HttpStreamPool::AttemptManager::stream_key() const {
  return group_->stream_key();
}

const SpdySessionKey& HttpStreamPool::AttemptManager::spdy_session_key() const {
  return group_->spdy_session_key();
}

const QuicSessionAliasKey&
HttpStreamPool::AttemptManager::quic_session_alias_key() const {
  return group_->quic_session_alias_key();
}

HttpNetworkSession* HttpStreamPool::AttemptManager::http_network_session()
    const {
  return group_->http_network_session();
}

SpdySessionPool* HttpStreamPool::AttemptManager::spdy_session_pool() const {
  return http_network_session()->spdy_session_pool();
}

QuicSessionPool* HttpStreamPool::AttemptManager::quic_session_pool() const {
  return http_network_session()->quic_session_pool();
}

HttpStreamPool* HttpStreamPool::AttemptManager::pool() {
  return group_->pool();
}

const HttpStreamPool* HttpStreamPool::AttemptManager::pool() const {
  return group_->pool();
}

int HttpStreamPool::AttemptManager::final_error_to_notify_jobs() const {
  CHECK(final_error_to_notify_jobs_.has_value());
  return *final_error_to_notify_jobs_;
}

const NetLogWithSource& HttpStreamPool::AttemptManager::net_log() {
  return net_log_;
}

bool HttpStreamPool::AttemptManager::UsingTls() const {
  return GURL::SchemeIsCryptographic(stream_key().destination().scheme());
}

bool HttpStreamPool::AttemptManager::RequiresHTTP11() {
  return pool()->RequiresHTTP11(stream_key().destination(),
                                stream_key().network_anonymization_key());
}

LoadState HttpStreamPool::AttemptManager::GetLoadState() const {
  if (group_->ReachedMaxStreamLimit()) {
    return LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET;
  }

  if (pool()->ReachedMaxStreamLimit()) {
    return LOAD_STATE_WAITING_FOR_STALLED_SOCKET_POOL;
  }

  LoadState load_state = LOAD_STATE_IDLE;

  // When there are TCP based attempts, use most advanced one.
  for (const auto& tcp_based_attempt : tcp_based_attempts_) {
    load_state =
        std::max(load_state, tcp_based_attempt->attempt()->GetLoadState());
    // There should not be a load state later than LOAD_STATE_SSL_HANDSHAKE.
    if (load_state == LOAD_STATE_SSL_HANDSHAKE) {
      break;
    }
  }

  if (load_state != LOAD_STATE_IDLE) {
    return load_state;
  }

  if (service_endpoint_request_ && !service_endpoint_request_finished_) {
    return LOAD_STATE_RESOLVING_HOST;
  }

  return LOAD_STATE_IDLE;
}

RequestPriority HttpStreamPool::AttemptManager::GetPriority() const {
  // There are several cases where `jobs_` is empty (e.g. `this` only has
  // preconnects, all jobs are already notified etc). Use IDLE for these cases.
  if (request_jobs_.empty()) {
    return RequestPriority::IDLE;
  }
  return static_cast<RequestPriority>(request_jobs_.FirstMax().priority());
}

bool HttpStreamPool::AttemptManager::IsStalledByPoolLimit() {
  if (is_shutting_down()) {
    return false;
  }

  if (!ip_endpoint_state_tracker_.GetIPEndPointToAttemptTcpBased()
           .has_value()) {
    return false;
  }

  if (CanUseExistingQuicSession()) {
    // There could be a matching QUIC session if an existing QUIC session
    // receives an HTTP/3 Origin frame while `this` is attempting QUIC session
    // establishment. In such case, QuicSessionAttempt will close the new
    // session later. See QuicSessionAttempt::DoConfirmConnection().
    return false;
  }

  if (HasAvailableSpdySession()) {
    CHECK_EQ(PendingPreconnectCount(), 0u);
    return false;
  }

  switch (CanAttemptConnection()) {
    case CanAttemptResult::kAttempt:
    case CanAttemptResult::kReachedPoolLimit:
      return true;
    case CanAttemptResult::kNoPendingJob:
    case CanAttemptResult::kBlockedTcpBasedAttempt:
    case CanAttemptResult::kThrottledForSpdy:
    case CanAttemptResult::kReachedGroupLimit:
      return false;
  }
}

void HttpStreamPool::AttemptManager::OnQuicAttemptComplete(
    QuicAttemptOutcome outcome) {
  CHECK(!quic_attempt_result_.has_value());
  int rv = outcome.result;
  QuicChromiumClientSession* quic_session = outcome.session;

  quic_attempt_result_ = rv;
  net_error_details_ = std::move(outcome.error_details);

  // Record completion time only when QuicAttempt actually attempted QUIC.
  if (rv != ERR_DNS_NO_MATCHING_SUPPORTED_ALPN) {
    base::UmaHistogramTimes(
        base::StrCat({"Net.HttpStreamPool.QuicAttemptTime.",
                      rv == OK ? "Success" : "Failure"}),
        base::TimeTicks::Now() - quic_attempt_->start_time());
  }

  quic_attempt_.reset();

  net_log().AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_QUIC_ATTEMPT_COMPLETED,
      [&] {
        base::Value::Dict dict = GetStatesAsNetLogParams();
        dict.Set("result", ErrorToString(rv));
        if (net_error_details_.quic_connection_error != quic::QUIC_NO_ERROR) {
          dict.Set("quic_error", quic::QuicErrorCodeToString(
                                     net_error_details_.quic_connection_error));
        }

        if (rv == OK) {
          CHECK(quic_session);
          quic_session->net_log().source().AddToEventParameters(dict);
        }
        return dict;
      });

  MaybeMarkQuicBroken();

  if (is_shutting_down()) {
    MaybeCompleteLater();
    return;
  }

  if (rv == OK) {
    HandleQuicSessionReady(quic_session,
                           StreamSocketCloseReason::kQuicSessionCreated);
    MaybeCompleteLater();
    return;
  }

  if (tcp_based_attempt_state_ == TcpBasedAttemptState::kAllEndpointsFailed ||
      !CanUseTcpBasedProtocols()) {
    CancelTcpBasedAttemptDelayTimer();
    HandleFinalError(rv);
    return;
  }

  if (should_block_tcp_based_attempt_) {
    CancelTcpBasedAttemptDelayTimer();
    MaybeAttemptTcpBased();
  } else {
    MaybeCompleteLater();
  }
}

void HttpStreamPool::AttemptManager::OnQuicAttemptSlow() {
  CHECK(quic_attempt_);
  CHECK(quic_attempt_->is_slow());
  if (is_shutting_down()) {
    CancelQuicAttempt(ERR_ABORTED);
    MaybeCompleteLater();
  }
}

base::Value::Dict HttpStreamPool::AttemptManager::GetInfoAsValue() const {
  base::Value::Dict dict;
  dict.Set("request_job_count_all", static_cast<int>(request_jobs_.size()));
  dict.Set("request_job_count_pending",
           static_cast<int>(PendingRequestJobCount()));
  dict.Set("job_count_limit_ignoring",
           static_cast<int>(limit_ignoring_jobs_.size()));
  dict.Set("job_count_notified", static_cast<int>(notified_jobs_.size()));
  dict.Set("preconnect_count_all", static_cast<int>(preconnect_jobs_.size()));
  dict.Set("preconnect_count_pending",
           static_cast<int>(PendingPreconnectCount()));
  dict.Set("tcp_based_attempt_count", static_cast<int>(TcpBasedAttemptCount()));
  dict.Set("slow_tcp_based_attempt_count",
           static_cast<int>(slow_tcp_based_attempt_count_));
  dict.Set("availability_state", static_cast<int>(availability_state_));
  if (final_error_to_notify_jobs_.has_value()) {
    dict.Set("final_error_to_notify_job", *final_error_to_notify_jobs_);
  }
  if (most_recent_tcp_error_.has_value()) {
    dict.Set("most_recent_tcp_error", *most_recent_tcp_error_);
  }
  dict.Set("can_attempt_connection",
           CanAttemptResultToString(CanAttemptConnection()));
  dict.Set("service_endpoint_request_finished",
           service_endpoint_request_finished_);
  dict.Set("tcp_based_attempt_state",
           TcpBasedAttemptStateToString(tcp_based_attempt_state_));
  dict.Set("tcp_based_attempt_delay_ms",
           static_cast<int>(tcp_based_attempt_delay_.InMilliseconds()));
  dict.Set("should_block_tcp_based_attempt", should_block_tcp_based_attempt_);

  int ssl_config_num_waiting_callbacks = 0;
  if (!tcp_based_attempts_.empty()) {
    base::Value::List tcp_based_attempts;
    for (const auto& entry : tcp_based_attempts_) {
      if (entry->IsWaitingSSLConfig()) {
        ++ssl_config_num_waiting_callbacks;
      }
      tcp_based_attempts.Append(entry->GetInfoAsValue());
    }
    dict.Set("tcp_based_attempts", std::move(tcp_based_attempts));
  }
  dict.Set("ssl_config_num_waiting_callbacks",
           ssl_config_num_waiting_callbacks);

  base::Value::List ip_endpoint_states =
      ip_endpoint_state_tracker_.GetInfoAsValue();
  if (!ip_endpoint_states.empty()) {
    dict.Set("ip_endpoint_states", std::move(ip_endpoint_states));
  }

  if (quic_attempt_) {
    dict.Set("quic_attempt", quic_attempt_->GetInfoAsValue());
  }
  if (quic_attempt_result_.has_value()) {
    dict.Set("quic_attempt_result", ErrorToString(*quic_attempt_result_));
  }

  return dict;
}

MultiplexedSessionCreationInitiator
HttpStreamPool::AttemptManager::CalculateMultiplexedSessionCreationInitiator() {
  // Iff we only have preconnect jobs, return `kPreconnect`.
  if (!preconnect_jobs_.empty() && request_jobs_.empty() &&
      notified_jobs_.empty()) {
    return MultiplexedSessionCreationInitiator::kPreconnect;
  }
  return MultiplexedSessionCreationInitiator::kUnknown;
}

void HttpStreamPool::AttemptManager::SetOnCompleteCallbackForTesting(
    base::OnceClosure callback) {
  CHECK(on_complete_callback_for_testing_.is_null());
  on_complete_callback_for_testing_ = std::move(callback);
}

void HttpStreamPool::AttemptManager::StartInternal(Job* job) {
  RestrictAllowedProtocols(job->allowed_alpns());
  UpdateTcpBasedAttemptState();

  if (service_endpoint_request_ || service_endpoint_request_finished_) {
    MaybeAttemptQuic();
    MaybeAttemptTcpBased();
  } else {
    ResolveServiceEndpoint(job->priority());
  }
}

void HttpStreamPool::AttemptManager::ResolveServiceEndpoint(
    RequestPriority initial_priority) {
  CHECK(!service_endpoint_request_);
  HostResolver::ResolveHostParameters parameters;
  parameters.initial_priority = initial_priority;
  parameters.secure_dns_policy = stream_key().secure_dns_policy();
  service_endpoint_request_ =
      http_network_session()->host_resolver()->CreateServiceEndpointRequest(
          HostResolver::Host(stream_key().destination()),
          stream_key().network_anonymization_key(), net_log(),
          std::move(parameters));

  dns_resolution_start_time_ = base::TimeTicks::Now();
  int rv = service_endpoint_request_->Start(this);
  if (rv != ERR_IO_PENDING) {
    OnServiceEndpointRequestFinished(rv);
  }
}

void HttpStreamPool::AttemptManager::RestrictAllowedProtocols(
    NextProtoSet allowed_alpns) {
  allowed_alpns_ = base::Intersection(allowed_alpns_, allowed_alpns);
  CHECK(!allowed_alpns_.empty());

  if (!CanUseTcpBasedProtocols()) {
    CancelTcpBasedAttempts(
        StreamSocketCloseReason::kCannotUseTcpBasedProtocols);
  }

  if (!CanUseQuic()) {
    // TODO(crbug.com/346835898): Use other error code?
    CancelQuicAttempt(ERR_ABORTED);
    UpdateTcpBasedAttemptState();
  }
}

void HttpStreamPool::AttemptManager::
    MaybeChangeServiceEndpointRequestPriority() {
  if (service_endpoint_request_ && !service_endpoint_request_finished_) {
    service_endpoint_request_->ChangeRequestPriority(GetPriority());
  }
}

void HttpStreamPool::AttemptManager::ProcessServiceEndpointChanges() {
  CHECK(availability_state_ == AvailabilityState::kAvailable);
  CHECK(service_endpoint_request_);

  // The order of the following checks is important, see the following comments.
  // TODO(crbug.com/383606724): Figure out a better design and algorithms to
  // handle attempts and existing sessions.

  // For plain HTTP request, we need to wait for HTTPS RR because we could
  // trigger HTTP -> HTTPS upgrade when HTTPS RR is received during the endpoint
  // resolution.
  if (!UsingTls() && !service_endpoint_request_->EndpointsCryptoReady() &&
      !service_endpoint_request_finished_) {
    return;
  }

  if (QuicChromiumClientSession* quic_session =
          CanUseExistingQuicSessionAfterEndpointChanges()) {
    net_log_.AddEvent(
        NetLogEventType::
            HTTP_STREAM_POOL_ATTEMPT_MANAGER_EXISTING_QUIC_SESSION_MATCHED,
        [&] {
          base::Value::Dict dict;
          quic_session->net_log().source().AddToEventParameters(dict);
          return dict;
        });
    base::UmaHistogramTimes(
        "Net.HttpStreamPool.ExistingQuicSessionFoundTime",
        base::TimeTicks::Now() - dns_resolution_start_time_);

    CancelQuicAttempt(OK);
    HandleQuicSessionReady(quic_session,
                           StreamSocketCloseReason::kUsingExistingQuicSession);

    CHECK(tcp_based_attempts_.empty());
    return;
  }

  if (base::WeakPtr<SpdySession> spdy_session =
          CanUseExistingSpdySessionAfterEndpointChanges()) {
    net_log_.AddEvent(
        NetLogEventType::
            HTTP_STREAM_POOL_ATTEMPT_MANAGER_EXISTING_SPDY_SESSION_MATCHED,
        [&] {
          base::Value::Dict dict;
          spdy_session->net_log().source().AddToEventParameters(dict);
          return dict;
        });
    base::UmaHistogramTimes(
        "Net.HttpStreamPool.ExistingSpdySessionFoundTime",
        base::TimeTicks::Now() - dns_resolution_start_time_);
    ip_matching_spdy_session_found_ = true;

    HandleSpdySessionReady(spdy_session,
                           StreamSocketCloseReason::kUsingExistingSpdySession);

    CHECK(tcp_based_attempts_.empty());
    return;
  }

  if (GetTcpBasedAttemptDelayBehavior() ==
      TcpBasedAttemptDelayBehavior::kStartTimerOnFirstEndpointUpdate) {
    MaybeRunTcpBasedAttemptDelayTimer();
  }

  MaybeNotifySSLConfigReady();
  MaybeAttemptQuic();
  MaybeAttemptTcpBased();
}

QuicChromiumClientSession* HttpStreamPool::AttemptManager::
    CanUseExistingQuicSessionAfterEndpointChanges() {
  if (!CanUseQuic()) {
    return nullptr;
  }

  if (CanUseExistingQuicSession()) {
    QuicChromiumClientSession* quic_session =
        quic_session_pool()->FindExistingSession(
            quic_session_alias_key().session_key(),
            quic_session_alias_key().destination());
    CHECK(quic_session);
    return quic_session;
  }

  for (const auto& endpoint : service_endpoint_request_->GetEndpointResults()) {
    QuicChromiumClientSession* quic_session =
        quic_session_pool()->HasMatchingIpSessionForServiceEndpoint(
            quic_session_alias_key(), endpoint,
            service_endpoint_request_->GetDnsAliasResults(), true);
    if (quic_session) {
      return quic_session;
    }
  }

  return nullptr;
}

base::WeakPtr<SpdySession> HttpStreamPool::AttemptManager::
    CanUseExistingSpdySessionAfterEndpointChanges() {
  if (!IsIpBasedPoolingEnabled() || !UsingTls()) {
    return nullptr;
  }

  if (HasAvailableSpdySession()) {
    base::WeakPtr<SpdySession> spdy_session = pool()->FindAvailableSpdySession(
        stream_key(), spdy_session_key(), IsIpBasedPoolingEnabled(), net_log());
    CHECK(spdy_session);
    CHECK(spdy_session->IsAvailable());
    return spdy_session;
  }

  for (const auto& endpoint : service_endpoint_request_->GetEndpointResults()) {
    base::WeakPtr<SpdySession> spdy_session =
        spdy_session_pool()->FindMatchingIpSessionForServiceEndpoint(
            spdy_session_key(), endpoint,
            service_endpoint_request_->GetDnsAliasResults());
    if (!spdy_session) {
      continue;
    }
    CHECK(spdy_session->IsAvailable());
    return spdy_session;
  }

  return nullptr;
}

void HttpStreamPool::AttemptManager::MaybeNotifySSLConfigReady() {
  if (!service_endpoint_request_->EndpointsCryptoReady()) {
    return;
  }

  // Collect callbacks from TCP based attempts and invoke them later.
  // Transferring callback ownership is important to avoid accessing TCP based
  // attempts that could be destroyed while invoking callbacks.
  std::vector<CompletionOnceCallback> callbacks;
  for (const auto& attempt : tcp_based_attempts_) {
    auto callback = attempt->MaybeTakeSSLConfigWaitingCallback();
    if (callback.has_value()) {
      callbacks.emplace_back(std::move(*callback));
    }
  }

  for (auto& callback : callbacks) {
    std::move(callback).Run(OK);
  }
}

void HttpStreamPool::AttemptManager::MaybeAttemptQuic() {
  if (is_shutting_down() || !CanUseQuic() || quic_attempt_result_.has_value()) {
    return;
  }

  CHECK(service_endpoint_request_);
  if (!service_endpoint_request_->EndpointsCryptoReady()) {
    return;
  }

  if (quic_attempt_) {
    // TODO(crbug.com/346835898): Support multiple QUIC attempts.
    return;
  }

  std::optional<QuicEndpoint> quic_endpoint = GetQuicEndpointToAttempt();
  if (quic_endpoint.has_value()) {
    quic_attempt_ =
        std::make_unique<QuicAttempt>(this, std::move(*quic_endpoint));
    quic_attempt_->Start();
    return;
  }

  if (service_endpoint_request_finished_) {
    // There is no QUIC endpoint to attempt.
    OnQuicAttemptComplete(
        QuicAttemptOutcome(ERR_DNS_NO_MATCHING_SUPPORTED_ALPN));
  }
}

void HttpStreamPool::AttemptManager::MaybeAttemptTcpBased(
    std::optional<IPEndPoint> exclude_ip_endpoint,
    std::optional<size_t> max_attempts) {
  if (is_shutting_down()) {
    return;
  }

  if (!CanUseTcpBasedProtocols()) {
    return;
  }

  if (CanUseQuic() && quic_attempt_result_.has_value() &&
      *quic_attempt_result_ == OK) {
    return;
  }

  // There might be multiple pending jobs. Make attempts as much as needed
  // and allowed.
  size_t num_attempts = 0;
  const bool using_tls = UsingTls();
  while (IsTcpBasedAttemptReady()) {
    // TODO(crbug.com/346835898): Change to DCHECK once we stabilize the
    // implementation.
    CHECK(!HasAvailableSpdySession());
    std::optional<IPEndPoint> ip_endpoint =
        ip_endpoint_state_tracker_.GetIPEndPointToAttemptTcpBased(
            exclude_ip_endpoint);
    if (!ip_endpoint.has_value()) {
      if (service_endpoint_request_finished_ && tcp_based_attempts_.empty()) {
        tcp_based_attempt_state_ = TcpBasedAttemptState::kAllEndpointsFailed;
      }
      if (tcp_based_attempt_state_ ==
              TcpBasedAttemptState::kAllEndpointsFailed &&
          !quic_attempt_) {
        // Tried all endpoints.
        CHECK(most_recent_tcp_error_.has_value());
        HandleFinalError(*most_recent_tcp_error_);
      }
      return;
    }

    if (tcp_based_attempt_state_ == TcpBasedAttemptState::kNotStarted) {
      SetInitialAttemptState();
      tcp_based_attempt_state_ = TcpBasedAttemptState::kAttempting;
    }

    CHECK(!preconnect_jobs_.empty() || group_->IdleStreamSocketCount() == 0);

    auto tcp_based_attempt =
        std::make_unique<TcpBasedAttempt>(this, using_tls, *ip_endpoint);
    auto [attempt_iterator, inserted] =
        tcp_based_attempts_.emplace(std::move(tcp_based_attempt));
    CHECK(inserted);
    pool()->IncrementTotalConnectingStreamCount();

    (*attempt_iterator)->Start();

    ++num_attempts;
    if (max_attempts.has_value() && num_attempts >= *max_attempts) {
      break;
    }
  }
}

bool HttpStreamPool::AttemptManager::IsTcpBasedAttemptReady() {
  switch (CanAttemptConnection()) {
    case CanAttemptResult::kAttempt:
      // If we ignore stream limits and the pool's limit has already reached,
      // try to close as much as possible.
      while (pool()->ReachedMaxStreamLimit()) {
        CHECK(!ShouldRespectLimits());
        if (!pool()->CloseOneIdleStreamSocket()) {
          break;
        }
      }
      return true;
    case CanAttemptResult::kNoPendingJob:
      return false;
    case CanAttemptResult::kBlockedTcpBasedAttempt:
      return false;
    case CanAttemptResult::kThrottledForSpdy:
      // TODO(crbug.com/346835898): Consider throttling less aggressively (e.g.
      // allow TCP handshake but throttle TLS handshake) so that endpoints we've
      // used HTTP/2 on aren't penalised on slow or lossy connections.
      if (!spdy_throttle_timer_.IsRunning()) {
        spdy_throttle_timer_.Start(
            FROM_HERE, kSpdyThrottleDelay,
            base::BindOnce(&AttemptManager::OnSpdyThrottleDelayPassed,
                           base::Unretained(this)));
      }
      return false;
    case CanAttemptResult::kReachedGroupLimit:
      // TODO(crbug.com/346835898): Better to handle cases where we partially
      // attempted some connections.
      NotifyPreconnectsComplete(ERR_PRECONNECT_MAX_SOCKET_LIMIT);
      return false;
    case CanAttemptResult::kReachedPoolLimit:
      // If we can't attempt connection due to the pool's limit, try to close an
      // idle stream in the pool.
      if (!pool()->CloseOneIdleStreamSocket()) {
        // Try to close idle SPDY sessions. SPDY sessions never release the
        // underlying sockets immediately on close, so return false anyway.
        spdy_session_pool()->CloseCurrentIdleSessions("Closing idle sessions");
        // TODO(crbug.com/346835898): Better to handle cases where we partially
        // attempted some connections.
        NotifyPreconnectsComplete(ERR_PRECONNECT_MAX_SOCKET_LIMIT);
        return false;
      }
      return true;
  }
}

HttpStreamPool::AttemptManager::CanAttemptResult
HttpStreamPool::AttemptManager::CanAttemptConnection() const {
  size_t pending_count =
      std::max(PendingRequestJobCount(), PendingPreconnectCount());
  if (pending_count == 0) {
    return CanAttemptResult::kNoPendingJob;
  }

  if (ShouldThrottleAttemptForSpdy()) {
    return CanAttemptResult::kThrottledForSpdy;
  }

  if (should_block_tcp_based_attempt_) {
    return CanAttemptResult::kBlockedTcpBasedAttempt;
  }

  if (ShouldRespectLimits()) {
    if (group_->ReachedMaxStreamLimit()) {
      return CanAttemptResult::kReachedGroupLimit;
    }

    if (pool()->ReachedMaxStreamLimit()) {
      return CanAttemptResult::kReachedPoolLimit;
    }
  }

  return CanAttemptResult::kAttempt;
}

bool HttpStreamPool::AttemptManager::ShouldRespectLimits() const {
  return limit_ignoring_jobs_.empty();
}

bool HttpStreamPool::AttemptManager::IsIpBasedPoolingEnabled() const {
  return ip_based_pooling_disabling_jobs_.empty();
}

bool HttpStreamPool::AttemptManager::IsAlternativeServiceEnabled() const {
  return alternative_service_disabling_jobs_.empty();
}

bool HttpStreamPool::AttemptManager::SupportsSpdy() const {
  return http_network_session()->http_server_properties()->GetSupportsSpdy(
      stream_key().destination(), stream_key().network_anonymization_key());
}

bool HttpStreamPool::AttemptManager::ShouldThrottleAttemptForSpdy() const {
  if (!SupportsSpdy()) {
    return false;
  }

  CHECK(UsingTls());

  // The first attempt should not be blocked.
  if (tcp_based_attempts_.empty()) {
    return false;
  }

  if (spdy_throttle_delay_passed_) {
    return false;
  }

  DCHECK(!HasAvailableSpdySession());
  return true;
}

size_t HttpStreamPool::AttemptManager::CalculateMaxPreconnectCount() const {
  size_t num_streams = 0;
  for (const auto& job : preconnect_jobs_) {
    num_streams = std::max(num_streams, job->num_streams());
  }
  return num_streams;
}

size_t HttpStreamPool::AttemptManager::PendingCountInternal(
    size_t pending_count) const {
  CHECK_GE(tcp_based_attempts_.size(), slow_tcp_based_attempt_count_);
  // When SPDY throttle delay passed, treat all in-flight attempts as non-slow,
  // to avoid attempting connections more than requested.
  // TODO(crbug.com/346835898): This behavior is tricky. Figure out a better
  // way to handle this situation.
  size_t slow_count =
      spdy_throttle_delay_passed_ ? 0 : slow_tcp_based_attempt_count_;
  size_t non_slow_count = tcp_based_attempts_.size() - slow_count;
  // The number of in-flight, non-slow attempts could be larger than the number
  // of jobs (e.g. a job was cancelled in the middle of an attempt).
  if (pending_count <= non_slow_count) {
    return 0;
  }

  return pending_count - non_slow_count;
}

std::optional<QuicEndpoint>
HttpStreamPool::AttemptManager::GetQuicEndpointToAttempt() {
  const bool svcb_optional = IsSvcbOptional();
  for (auto& service_endpoint :
       service_endpoint_request()->GetEndpointResults()) {
    quic::ParsedQuicVersion endpoint_quic_version =
        quic_session_pool()->SelectQuicVersion(
            quic_version_, service_endpoint.metadata, svcb_optional);
    if (!endpoint_quic_version.IsKnown()) {
      continue;
    }

    // TODO(crbug.com/346835898): Attempt more than one endpoints.
    std::optional<IPEndPoint> ip_endpoint;
    if (!service_endpoint.ipv6_endpoints.empty()) {
      ip_endpoint = service_endpoint.ipv6_endpoints[0];
    } else if (!service_endpoint.ipv4_endpoints.empty()) {
      ip_endpoint = service_endpoint.ipv4_endpoints[0];
    }

    if (!ip_endpoint.has_value()) {
      continue;
    }

    return QuicEndpoint(endpoint_quic_version, *ip_endpoint,
                        service_endpoint.metadata);
  }

  return std::nullopt;
}

void HttpStreamPool::AttemptManager::HandleFinalError(int error) {
  // `this` may already be failing, e.g. IP address change happens while failing
  // for a different reason.
  if (availability_state_ == AvailabilityState::kFailing) {
    return;
  }

  CHECK(!final_error_to_notify_jobs_.has_value());
  final_error_to_notify_jobs_ = error;
  availability_state_ = AvailabilityState::kFailing;
  service_endpoint_request_.reset();

  net_log_.AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_NOTIFY_FAILURE, [&] {
        base::Value::Dict dict = GetStatesAsNetLogParams();
        dict.Set("net_error", final_error_to_notify_jobs());
        return dict;
      });

  CancelTcpBasedAttempts(StreamSocketCloseReason::kAbort);
  CancelQuicAttempt(final_error_to_notify_jobs());
  NotifyPreconnectsComplete(final_error_to_notify_jobs());
  NotifyJobOfFailure();

  CHECK(tcp_based_attempts_.empty());
  CHECK(request_jobs_.empty());
  CHECK(preconnect_jobs_.empty());
  CHECK(!quic_attempt_);

  group_->OnAttemptManagerShuttingDown(this);
  // `this` may be deleted.
}

HttpStreamPool::AttemptManager::FailureKind
HttpStreamPool::AttemptManager::DetermineFailureKind() {
  if (IsCertificateError(final_error_to_notify_jobs())) {
    return FailureKind::kCertifcateError;
  }

  if (final_error_to_notify_jobs() == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    return FailureKind::kNeedsClientAuth;
  }

  return FailureKind::kStreamFailed;
}

void HttpStreamPool::AttemptManager::NotifyJobOfFailure() {
  CHECK_EQ(availability_state_, AvailabilityState::kFailing);

  const FailureKind kind = DetermineFailureKind();
  base::WeakPtr<AttemptManager> weak_this = weak_ptr_factory_.GetWeakPtr();
  while (Job* job = ExtractFirstJobToNotify()) {
    // Ensure `this` isn't deleted while notifying the failure.
    // TODO(crbug.com/414173943): Remove this check when we are certain that
    // `this` won't be deleted.
    CHECK(weak_this);

    job->AddConnectionAttempts(connection_attempts_);

    switch (kind) {
      case FailureKind::kStreamFailed: {
        TRACE_EVENT_INSTANT("net.stream", "AttemptManager::StreamFailed",
                            track_,
                            NetLogWithSourceToFlow(job->request_net_log()));
        job->OnStreamFailed(final_error_to_notify_jobs(), net_error_details_,
                            resolve_error_info_);
        break;
      }
      case FailureKind::kCertifcateError: {
        CHECK(cert_error_ssl_info_.has_value());
        TRACE_EVENT_INSTANT("net.stream", "AttemptManager::CertificateError",
                            track_,
                            NetLogWithSourceToFlow(job->request_net_log()));
        job->OnCertificateError(final_error_to_notify_jobs(),
                                *cert_error_ssl_info_);
        break;
      }
      case FailureKind::kNeedsClientAuth: {
        CHECK(client_auth_cert_info_.get());
        TRACE_EVENT_INSTANT("net.stream", "AttemptManager::NeedsClientAuth",
                            track_,
                            NetLogWithSourceToFlow(job->request_net_log()));
        job->OnNeedsClientAuth(client_auth_cert_info_.get());
        break;
      }
    }
  }
}

void HttpStreamPool::AttemptManager::NotifyPreconnectsComplete(int rv) {
  while (!preconnect_jobs_.empty()) {
    raw_ptr<Job> job =
        preconnect_jobs_.extract(preconnect_jobs_.begin()).value();
    NotifyJobOfPreconnectComplete(std::move(job), rv);
  }

  // TODO(crbug.com/414173943): Start draining if there is no request/preconnect
  // jobs.

  // TODO(crbug.com/396998469): Do we still need this? Remove if this is not
  // needed.
  MaybeCompleteLater();
}

void HttpStreamPool::AttemptManager::ProcessPreconnectsAfterAttemptComplete(
    int rv,
    size_t active_stream_count) {
  std::vector<Job*> completed_jobs;
  for (auto& job : preconnect_jobs_) {
    if (job->num_streams() <= active_stream_count) {
      completed_jobs.emplace_back(job.get());
    }
  }

  for (auto* completed_job : completed_jobs) {
    auto it = preconnect_jobs_.find(completed_job);
    CHECK(it != preconnect_jobs_.end());
    raw_ptr<Job> job = preconnect_jobs_.extract(it).value();
    NotifyJobOfPreconnectComplete(std::move(job), rv);
  }

  // TODO(crbug.com/414173943): Start draining if there is no request/preconnect
  // jobs.

  // TODO(crbug.com/396998469): Do we still need this? Remove if this is not
  // needed.
  if (preconnect_jobs_.empty()) {
    MaybeCompleteLater();
  }
}

void HttpStreamPool::AttemptManager::NotifyJobOfPreconnectComplete(
    raw_ptr<Job> job,
    int rv) {
  Job* raw_job = job.get();
  notified_jobs_.emplace(std::move(job));
  TRACE_EVENT_INSTANT("net.stream",
                      "AttemptManager::NotifyJobOfPreconnectComplete", track_,
                      NetLogWithSourceToFlow(raw_job->request_net_log()));
  // We don't need to call MaybeCompleteLater() here, since `job` will call
  // OnJobComplete() later.
  raw_job->OnPreconnectComplete(rv);
}

void HttpStreamPool::AttemptManager::CreateTextBasedStreamAndNotify(
    std::unique_ptr<StreamSocket> stream_socket,
    StreamSocketHandle::SocketReuseType reuse_type,
    LoadTimingInfo::ConnectTiming connect_timing) {
  CHECK(!request_jobs_.empty());

  NextProto negotiated_protocol = stream_socket->GetNegotiatedProtocol();
  CHECK_NE(negotiated_protocol, NextProto::kProtoHTTP2);

  std::unique_ptr<HttpStream> http_stream = group_->CreateTextBasedStream(
      std::move(stream_socket), reuse_type, std::move(connect_timing));
  CHECK(!ShouldRespectLimits() || group_->ActiveStreamSocketCount() <=
                                      pool()->max_stream_sockets_per_group())
      << "active=" << group_->ActiveStreamSocketCount()
      << ", limit=" << pool()->max_stream_sockets_per_group();

  NotifyStreamReady(std::move(http_stream), negotiated_protocol);
}

bool HttpStreamPool::AttemptManager::HasAvailableSpdySession() const {
  return spdy_session_pool()->HasAvailableSession(
      spdy_session_key(), IsIpBasedPoolingEnabled(), /*is_websocket=*/false);
}

void HttpStreamPool::AttemptManager::MaybeStartDraining() {
  if (!request_jobs_.empty() || !preconnect_jobs_.empty()) {
    return;
  }

  CHECK_EQ(availability_state_, AvailabilityState::kAvailable);
  CHECK(!CanComplete());
  availability_state_ = AvailabilityState::kDraining;
  service_endpoint_request_.reset();

  // Cancel in-flight TCP based attempts so that draining AttemptManager won't
  // have active connecting streams.
  // TODO(crbug.com/414173943): It might be better not to cancel in-flight
  // TCP based attempts for future requests/preconnects unless these aren't
  // slow. Currently we just cancel them for simplicity. If we want to keep
  // these attempts in the draining `this`, Group::ConnectingStreamSocketCount()
  // should check draining AttemptManagers.
  CancelTcpBasedAttempts(StreamSocketCloseReason::kAbort);

  if (quic_attempt_ && quic_attempt_->is_slow()) {
    CancelQuicAttempt(ERR_ABORTED);
  }

  group_->OnAttemptManagerShuttingDown(this);
}

void HttpStreamPool::AttemptManager::MaybeCreateSpdyStreamAndNotify(
    base::WeakPtr<SpdySession> spdy_session) {
  if (request_jobs_.empty()) {
    return;
  }

  CHECK(availability_state_ == AvailabilityState::kAvailable);
  CHECK(spdy_session);
  CHECK(spdy_session->IsAvailable());

  std::set<std::string> dns_aliases =
      http_network_session()->spdy_session_pool()->GetDnsAliasesForSessionKey(
          spdy_session_key());

  std::vector<std::unique_ptr<SpdyHttpStream>> streams(request_jobs_.size());
  std::ranges::generate(streams, [&] {
    return std::make_unique<SpdyHttpStream>(spdy_session, net_log().source(),
                                            dns_aliases);
  });

  // This WeakPtr is to ensure `this` is not destroyed while notifying.
  // TODO(crbug.com/417339803): Remove once we stabilize the implementation.
  base::WeakPtr<AttemptManager> weak_this = weak_ptr_factory_.GetWeakPtr();
  while (!streams.empty()) {
    std::unique_ptr<SpdyHttpStream> stream = std::move(streams.back());
    streams.pop_back();
    NotifyStreamReady(std::move(stream), NextProto::kProtoHTTP2);
    CHECK(weak_this);
  }
  CHECK(request_jobs_.empty());
}

void HttpStreamPool::AttemptManager::MaybeCreateQuicStreamAndNotify(
    QuicChromiumClientSession* quic_session) {
  if (request_jobs_.empty()) {
    return;
  }

  CHECK(availability_state_ == AvailabilityState::kAvailable);
  CHECK(quic_session);

  std::set<std::string> dns_aliases = quic_session->GetDnsAliasesForSessionKey(
      quic_session_alias_key().session_key());

  std::vector<std::unique_ptr<QuicHttpStream>> streams(request_jobs_.size());
  std::ranges::generate(streams, [&] {
    return std::make_unique<QuicHttpStream>(
        quic_session->CreateHandle(stream_key().destination()), dns_aliases);
  });

  // This WeakPtr is to ensure `this` is not destroyed while notifying.
  // TODO(crbug.com/417339803): Remove once we stabilize the implementation.
  base::WeakPtr<AttemptManager> weak_this = weak_ptr_factory_.GetWeakPtr();
  while (!streams.empty()) {
    std::unique_ptr<QuicHttpStream> stream = std::move(streams.back());
    streams.pop_back();
    NotifyStreamReady(std::move(stream), NextProto::kProtoQUIC);
    CHECK(weak_this);
  }
  CHECK(request_jobs_.empty());
}

void HttpStreamPool::AttemptManager::NotifyStreamReady(
    std::unique_ptr<HttpStream> stream,
    NextProto negotiated_protocol) {
  Job* job = ExtractFirstJobToNotify();
  CHECK(job);
  TRACE_EVENT_INSTANT("net.stream", "AttemptManager::NotifyStreamReady", track_,
                      NetLogWithSourceToFlow(job->request_net_log()),
                      "negotiated_protocol", negotiated_protocol);
  job->OnStreamReady(std::move(stream), negotiated_protocol);
  MaybeStartDraining();
}

void HttpStreamPool::AttemptManager::HandleSpdySessionReady(
    base::WeakPtr<SpdySession> spdy_session,
    StreamSocketCloseReason refresh_group_reason) {
  CHECK(!group_->force_quic());
  CHECK(availability_state_ == AvailabilityState::kAvailable);
  CHECK(spdy_session);
  CHECK(spdy_session->IsAvailable());

  TRACE_EVENT_INSTANT("net.stream", "AttemptManager::SpdySessionReady", track_);

  group_->Refresh(kSwitchingToHttp2, refresh_group_reason);
  NotifyPreconnectsComplete(OK);
  MaybeCreateSpdyStreamAndNotify(spdy_session);
}

void HttpStreamPool::AttemptManager::HandleQuicSessionReady(
    QuicChromiumClientSession* quic_session,
    StreamSocketCloseReason refresh_group_reason) {
  CHECK(availability_state_ == AvailabilityState::kAvailable);
  CHECK(!quic_attempt_);
  CHECK(quic_session);
  // TODO(crbug.com/415488524): Change to DCHECK once we confirm the bug is
  // fixed.
  CHECK(CanUseExistingQuicSession());

  TRACE_EVENT_INSTANT("net.stream", "AttemptManager::QuicSessionReady", track_);

  group_->Refresh(kSwitchingToHttp3, refresh_group_reason);
  NotifyPreconnectsComplete(OK);
  MaybeCreateQuicStreamAndNotify(quic_session);
}

HttpStreamPool::Job* HttpStreamPool::AttemptManager::ExtractFirstJobToNotify() {
  if (request_jobs_.empty()) {
    return nullptr;
  }
  raw_ptr<Job> job = RemoveJobFromQueue(request_jobs_.FirstMax());
  Job* job_raw_ptr = job.get();
  notified_jobs_.emplace(std::move(job));
  return job_raw_ptr;
}

raw_ptr<HttpStreamPool::Job> HttpStreamPool::AttemptManager::RemoveJobFromQueue(
    JobQueue::Pointer job_pointer) {
  // If the extracted job is the last job that ignores the limit, cancel
  // in-flight attempts until the active stream count goes down to the limit.
  raw_ptr<Job> job = request_jobs_.Erase(job_pointer);
  limit_ignoring_jobs_.erase(job);
  if (ShouldRespectLimits()) {
    while (group_->ActiveStreamSocketCount() >
               pool()->max_stream_sockets_per_group() &&
           !tcp_based_attempts_.empty()) {
      std::unique_ptr<TcpBasedAttempt> attempt =
          ExtractTcpBasedAttempt(tcp_based_attempts_.begin()->get());
      attempt.reset();
    }
  }
  return job;
}

void HttpStreamPool::AttemptManager::SetJobPriority(Job* job,
                                                    RequestPriority priority) {
  for (JobQueue::Pointer pointer = request_jobs_.FirstMax(); !pointer.is_null();
       pointer = request_jobs_.GetNextTowardsLastMin(pointer)) {
    if (pointer.value() == job) {
      if (pointer.priority() == priority) {
        break;
      }

      raw_ptr<Job> entry = request_jobs_.Erase(pointer);
      request_jobs_.Insert(std::move(entry), priority);
      break;
    }
  }

  MaybeChangeServiceEndpointRequestPriority();
}

std::unique_ptr<HttpStreamPool::AttemptManager::TcpBasedAttempt>
HttpStreamPool::AttemptManager::ExtractTcpBasedAttempt(
    TcpBasedAttempt* raw_attempt) {
  auto it = tcp_based_attempts_.find(raw_attempt);
  CHECK(it != tcp_based_attempts_.end());
  std::unique_ptr<TcpBasedAttempt> attempt =
      std::move(tcp_based_attempts_.extract(it).value());

  pool()->DecrementTotalConnectingStreamCount();
  if (attempt->is_slow()) {
    CHECK_GT(slow_tcp_based_attempt_count_, 0u);
    --slow_tcp_based_attempt_count_;
  }

  return attempt;
}

void HttpStreamPool::AttemptManager::OnTcpBasedAttemptComplete(
    TcpBasedAttempt* raw_attempt,
    int rv) {
  if (rv == OK && raw_attempt->is_slow()) {
    ip_endpoint_state_tracker_.OnEndpointSlowSucceeded(
        raw_attempt->ip_endpoint());
  }

  std::unique_ptr<TcpBasedAttempt> tcp_based_attempt =
      ExtractTcpBasedAttempt(raw_attempt);

  if (rv != OK) {
    HandleTcpBasedAttemptFailure(std::move(tcp_based_attempt), rv);
    return;
  }

  CHECK_NE(tcp_based_attempt_state_, TcpBasedAttemptState::kAllEndpointsFailed);
  if (tcp_based_attempt_state_ == TcpBasedAttemptState::kAttempting) {
    tcp_based_attempt_state_ = TcpBasedAttemptState::kSucceededAtLeastOnce;
    MaybeMarkQuicBroken();
  }

  LoadTimingInfo::ConnectTiming connect_timing =
      tcp_based_attempt->attempt()->connect_timing();
  connect_timing.domain_lookup_start = dns_resolution_start_time_;
  // If the attempt started before DNS resolution completion, `connect_start`
  // could be smaller than `dns_resolution_end_time_`. Use the smallest one.
  connect_timing.domain_lookup_end =
      dns_resolution_end_time_.is_null()
          ? connect_timing.connect_start
          : std::min(connect_timing.connect_start, dns_resolution_end_time_);

  std::unique_ptr<StreamSocket> stream_socket =
      tcp_based_attempt->attempt()->ReleaseStreamSocket();
  CHECK(stream_socket);
  CHECK(service_endpoint_request_);
  stream_socket->SetDnsAliases(service_endpoint_request_->GetDnsAliasResults());

  spdy_throttle_timer_.Stop();

  const auto reuse_type = StreamSocketHandle::SocketReuseType::kUnused;
  if (stream_socket->GetNegotiatedProtocol() == NextProto::kProtoHTTP2) {
    std::unique_ptr<HttpStreamPoolHandle> handle = group_->CreateHandle(
        std::move(stream_socket), reuse_type, std::move(connect_timing));
    base::WeakPtr<SpdySession> spdy_session;
    int create_result =
        spdy_session_pool()->CreateAvailableSessionFromSocketHandle(
            spdy_session_key(), std::move(handle), net_log(),
            MultiplexedSessionCreationInitiator::kUnknown, &spdy_session,
            SpdySessionInitiator::kHttpStreamPoolAttemptManager);
    if (create_result != OK) {
      HandleTcpBasedAttemptFailure(std::move(tcp_based_attempt), create_result);
      return;
    }

    HttpServerProperties* http_server_properties =
        http_network_session()->http_server_properties();
    http_server_properties->SetSupportsSpdy(
        stream_key().destination(), stream_key().network_anonymization_key(),
        /*supports_spdy=*/true);

    base::UmaHistogramTimes(
        "Net.HttpStreamPool.NewSpdySessionEstablishTime",
        base::TimeTicks::Now() - tcp_based_attempt->start_time());

    HandleSpdySessionReady(spdy_session,
                           StreamSocketCloseReason::kSpdySessionCreated);
    return;
  }

  // We will create an active stream so +1 to the current active stream count.
  ProcessPreconnectsAfterAttemptComplete(rv,
                                         group_->ActiveStreamSocketCount() + 1);

  // If there is no request job, put the stream as an idle stream and try to
  // process pending requests in the group/pool.
  if (request_jobs_.empty()) {
    group_->AddIdleStreamSocket(std::move(stream_socket));
    pool()->ProcessPendingRequestsInGroups();
  } else {
    CreateTextBasedStreamAndNotify(std::move(stream_socket), reuse_type,
                                   std::move(connect_timing));
  }
}

void HttpStreamPool::AttemptManager::OnTcpBasedAttemptSlow(
    TcpBasedAttempt* raw_attempt) {
  auto it = tcp_based_attempts_.find(raw_attempt);
  CHECK(it != tcp_based_attempts_.end());

  raw_attempt->set_is_slow(true);
  ++slow_tcp_based_attempt_count_;
  ip_endpoint_state_tracker_.OnEndpointSlow(raw_attempt->ip_endpoint());

  // Don't attempt the same IP endpoint.
  MaybeAttemptTcpBased(/*exclude_ip_endpoint=*/raw_attempt->ip_endpoint());
}

void HttpStreamPool::AttemptManager::HandleTcpBasedAttemptFailure(
    std::unique_ptr<TcpBasedAttempt> tcp_based_attempt,
    int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  ip_endpoint_state_tracker_.OnEndpointFailed(tcp_based_attempt->ip_endpoint());
  connection_attempts_.emplace_back(tcp_based_attempt->ip_endpoint(), rv);

  if (tcp_based_attempt->is_aborted()) {
    CHECK_EQ(rv, ERR_ABORTED);
    // TODO(crbug.com/403373872): Reduce this failure.
    most_recent_tcp_error_ = ERR_ABORTED;
    return;
  }

  // We already removed `tcp_based_attempt` from `tcp_based_attempts_` so
  // the active stream count is up-to-date.
  ProcessPreconnectsAfterAttemptComplete(rv, group_->ActiveStreamSocketCount());

  if (is_shutting_down()) {
    // `this` has already failed and is notifying jobs to the failure.
    return;
  }

  if (rv == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    CHECK(UsingTls());
    client_auth_cert_info_ = tcp_based_attempt->attempt()->GetCertRequestInfo();
    tcp_based_attempt.reset();
    HandleFinalError(rv);
    return;
  }

  if (IsCertificateError(rv)) {
    // When a certificate error happened for an attempt, notifies all jobs of
    // the error.
    CHECK(UsingTls());
    CHECK(tcp_based_attempt->attempt()->stream_socket());
    SSLInfo ssl_info;
    bool has_ssl_info =
        tcp_based_attempt->attempt()->stream_socket()->GetSSLInfo(&ssl_info);
    CHECK(has_ssl_info);
    cert_error_ssl_info_ = ssl_info;
    tcp_based_attempt.reset();
    HandleFinalError(rv);
    return;
  }

  most_recent_tcp_error_ = rv;
  tcp_based_attempt.reset();
  // Try to connect to a different destination, if any.
  // TODO(crbug.com/383606724): Figure out better way to make connection
  // attempts, see the review comment at
  // https://chromium-review.googlesource.com/c/chromium/src/+/6160855/comment/60e04065_805b0b89/
  MaybeAttemptTcpBased();
}

void HttpStreamPool::AttemptManager::OnSpdyThrottleDelayPassed() {
  CHECK(!spdy_throttle_delay_passed_);
  spdy_throttle_delay_passed_ = true;
  MaybeAttemptTcpBased();
}

base::TimeDelta HttpStreamPool::AttemptManager::GetTcpBasedAttemptDelay() {
  if (!CanUseQuic()) {
    return base::TimeDelta();
  }

  return quic_session_pool()->GetTimeDelayForWaitingJob(
      quic_session_alias_key().session_key());
}

void HttpStreamPool::AttemptManager::UpdateTcpBasedAttemptState() {
  if (should_block_tcp_based_attempt_ && !CanUseQuic()) {
    CancelTcpBasedAttemptDelayTimer();
  }
}

void HttpStreamPool::AttemptManager::MaybeRunTcpBasedAttemptDelayTimer() {
  if (!should_block_tcp_based_attempt_ ||
      tcp_based_attempt_delay_timer_.IsRunning() ||
      !CanUseTcpBasedProtocols()) {
    return;
  }
  CHECK(!tcp_based_attempt_delay_.is_zero());
  tcp_based_attempt_delay_timer_.Start(
      FROM_HERE, tcp_based_attempt_delay_,
      base::BindOnce(&AttemptManager::OnTcpBasedAttemptDelayPassed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HttpStreamPool::AttemptManager::CancelTcpBasedAttemptDelayTimer() {
  should_block_tcp_based_attempt_ = false;
  tcp_based_attempt_delay_timer_.Stop();
}

void HttpStreamPool::AttemptManager::OnTcpBasedAttemptDelayPassed() {
  net_log().AddEvent(
      NetLogEventType::
          HTTP_STREAM_POOL_ATTEMPT_MANAGER_TCP_BASED_ATTEMPT_DELAY_PASSED,
      [&] {
        base::Value::Dict dict;
        dict.Set("tcp_based_attempt_delay",
                 static_cast<int>(tcp_based_attempt_delay_.InMilliseconds()));
        return dict;
      });
  CHECK(should_block_tcp_based_attempt_);
  should_block_tcp_based_attempt_ = false;
  MaybeAttemptTcpBased();
}

bool HttpStreamPool::AttemptManager::CanUseTcpBasedProtocols() {
  return allowed_alpns_.HasAny(kTcpBasedProtocols);
}

bool HttpStreamPool::AttemptManager::CanUseQuic() {
  return allowed_alpns_.HasAny(kQuicBasedProtocols) &&
         pool()->CanUseQuic(stream_key().destination(),
                            stream_key().network_anonymization_key(),
                            IsIpBasedPoolingEnabled(),
                            IsAlternativeServiceEnabled());
}

bool HttpStreamPool::AttemptManager::CanUseExistingQuicSession() {
  return pool()->CanUseExistingQuicSession(quic_session_alias_key(),
                                           IsIpBasedPoolingEnabled(),
                                           IsAlternativeServiceEnabled());
}

bool HttpStreamPool::AttemptManager::IsEchEnabled() const {
  return pool()
      ->stream_attempt_params()
      ->ssl_client_context->config()
      .ech_enabled;
}

void HttpStreamPool::AttemptManager::MaybeMarkQuicBroken() {
  if (!quic_attempt_result_.has_value() ||
      tcp_based_attempt_state_ == TcpBasedAttemptState::kAttempting) {
    return;
  }

  if (*quic_attempt_result_ == OK ||
      *quic_attempt_result_ == ERR_DNS_NO_MATCHING_SUPPORTED_ALPN ||
      *quic_attempt_result_ == ERR_NETWORK_CHANGED ||
      *quic_attempt_result_ == ERR_INTERNET_DISCONNECTED) {
    return;
  }

  // No brokenness to report if we didn't attempt TCP-based connection or all
  // TCP-based attempts failed.
  if (tcp_based_attempt_state_ == TcpBasedAttemptState::kNotStarted ||
      tcp_based_attempt_state_ == TcpBasedAttemptState::kAllEndpointsFailed) {
    return;
  }

  const url::SchemeHostPort& destination = stream_key().destination();
  http_network_session()
      ->http_server_properties()
      ->MarkAlternativeServiceBroken(
          AlternativeService(NextProto::kProtoQUIC, destination.host(),
                             destination.port()),
          stream_key().network_anonymization_key());
}

base::Value::Dict HttpStreamPool::AttemptManager::GetStatesAsNetLogParams()
    const {
  if (VerboseNetLog()) {
    return GetInfoAsValue();
  }

  base::Value::Dict dict;
  dict.Set("num_active_sockets",
           static_cast<int>(group_->ActiveStreamSocketCount()));
  dict.Set("num_idle_sockets",
           static_cast<int>(group_->IdleStreamSocketCount()));
  dict.Set("num_total_sockets",
           static_cast<int>(group_->ActiveStreamSocketCount()));
  dict.Set("num_jobs", static_cast<int>(request_jobs_.size()));
  dict.Set("num_notified_jobs", static_cast<int>(notified_jobs_.size()));
  dict.Set("num_preconnects", static_cast<int>(preconnect_jobs_.size()));
  dict.Set("num_tcp_based_attempts",
           static_cast<int>(tcp_based_attempts_.size()));
  dict.Set("num_slow_attempts",
           static_cast<int>(slow_tcp_based_attempt_count_));
  dict.Set("enable_ip_based_pooling", IsIpBasedPoolingEnabled());
  dict.Set("enable_alternative_services", IsAlternativeServiceEnabled());
  dict.Set("quic_attempt_alive", !!quic_attempt_);
  if (quic_attempt_result_.has_value()) {
    dict.Set("quic_attempt_result", ErrorToString(*quic_attempt_result_));
  }
  return dict;
}

bool HttpStreamPool::AttemptManager::CanComplete() const {
  return request_jobs_.empty() && notified_jobs_.empty() &&
         preconnect_jobs_.empty() && tcp_based_attempts_.empty() &&
         !quic_attempt_;
}

void HttpStreamPool::AttemptManager::MaybeComplete() {
  if (!CanComplete()) {
    return;
  }

  CHECK(limit_ignoring_jobs_.empty());
  CHECK(ip_based_pooling_disabling_jobs_.empty());
  CHECK(alternative_service_disabling_jobs_.empty());

  if (on_complete_callback_for_testing_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_complete_callback_for_testing_));
  }

  group_->OnAttemptManagerComplete(this);
  // `this` is deleted.
}

void HttpStreamPool::AttemptManager::MaybeCompleteLater() {
  if (CanComplete()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AttemptManager::MaybeComplete,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

}  // namespace net

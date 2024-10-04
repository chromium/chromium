// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_attempt_manager.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool_group.h"
#include "net/http/http_stream_pool_handle.h"
#include "net/http/http_stream_pool_job.h"
#include "net/http/http_stream_pool_quic_task.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_attempt.h"
#include "net/socket/stream_socket_handle.h"
#include "net/socket/tcp_stream_attempt.h"
#include "net/socket/tls_stream_attempt.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
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

}  // namespace

// Represents an in-flight stream attempt.
class HttpStreamPool::AttemptManager::InFlightAttempt
    : public TlsStreamAttempt::SSLConfigProvider {
 public:
  explicit InFlightAttempt(AttemptManager* manager) : manager_(manager) {}

  InFlightAttempt(const InFlightAttempt&) = delete;
  InFlightAttempt& operator=(const InFlightAttempt&) = delete;

  ~InFlightAttempt() override = default;

  int Start(std::unique_ptr<StreamAttempt> attempt) {
    CHECK(!attempt_);
    attempt_ = std::move(attempt);
    // SAFETY: `manager_` owns `this` so using base::Unretained() is safe.
    return attempt_->Start(
        base::BindOnce(&AttemptManager::OnInFlightAttemptComplete,
                       base::Unretained(manager_), this));
  }

  StreamAttempt* attempt() { return attempt_.get(); }

  bool is_slow() const { return is_slow_; }
  void set_is_slow(bool is_slow) { is_slow_ = is_slow; }

  base::OneShotTimer& slow_timer() { return slow_timer_; }

  // TlsStreamAttempt::SSLConfigProvider implementation:
  int WaitForSSLConfigReady(CompletionOnceCallback callback) override {
    return manager_->WaitForSSLConfigReady(std::move(callback));
  }

  SSLConfig GetSSLConfig() override { return manager_->GetSSLConfig(); }

 private:
  const raw_ptr<AttemptManager> manager_;
  std::unique_ptr<StreamAttempt> attempt_;
  // Timer to start a next attempt. When fired, `this` is treated as a slow
  // attempt but `this` is not timed out yet.
  base::OneShotTimer slow_timer_;
  bool is_slow_ = false;
};

// Represents a preconnect request.
struct HttpStreamPool::AttemptManager::PreconnectEntry {
  PreconnectEntry(size_t num_streams, CompletionOnceCallback callback)
      : num_streams(num_streams), callback(std::move(callback)) {}

  PreconnectEntry(const PreconnectEntry&) = delete;
  PreconnectEntry& operator=(const PreconnectEntry&) = delete;

  ~PreconnectEntry() = default;

  size_t num_streams;
  CompletionOnceCallback callback;
  // Set to the latest error when errors happened.
  int result = OK;
};

HttpStreamPool::AttemptManager::AttemptManager(Group* group, NetLog* net_log)
    : group_(group),
      net_log_(NetLogWithSource::Make(
          net_log,
          NetLogSourceType::HTTP_STREAM_POOL_ATTEMPT_MANAGER)),
      jobs_(NUM_PRIORITIES),
      stream_attempt_delay_(GetStreamAttemptDelay()),
      should_block_stream_attempt_(!stream_attempt_delay_.is_zero()) {
  CHECK(group_);
  net_log_.BeginEvent(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_ALIVE, [&] {
        base::Value::Dict dict;
        dict.Set("stream_attempt_delay",
                 static_cast<int>(stream_attempt_delay_.InMilliseconds()));
        group_->net_log().source().AddToEventParameters(dict);
        return dict;
      });
  group_->net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_GROUP_ATTEMPT_MANAGER_CREATED,
      net_log_.source());
}

HttpStreamPool::AttemptManager::~AttemptManager() {
  net_log().EndEvent(NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_ALIVE);
  group_->net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_GROUP_ATTEMPT_MANAGER_DESTROYED,
      net_log_.source());
}

void HttpStreamPool::AttemptManager::StartJob(
    Job* job,
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    RespectLimits respect_limits,
    bool enable_ip_based_pooling,
    bool enable_alternative_services,
    quic::ParsedQuicVersion quic_version,
    const NetLogWithSource& net_log) {
  if (respect_limits == RespectLimits::kIgnore) {
    respect_limits_ = RespectLimits::kIgnore;
  }

  if (!enable_ip_based_pooling) {
    enable_ip_based_pooling_ = enable_ip_based_pooling;
  }

  if (!enable_alternative_services) {
    enable_alternative_services_ = enable_alternative_services;
  }

  // HttpStreamPool should check the existing QUIC/SPDY sessions before calling
  // this method.
  DCHECK(!CanUseExistingQuicSession());
  CHECK(!spdy_session_);
  DCHECK(!spdy_session_pool()->FindAvailableSession(
      spdy_session_key(), enable_ip_based_pooling_,
      /*is_websocket=*/false, net_log));

  jobs_.Insert(job, priority);

  if (is_failing_) {
    // `this` is failing, notify the failure.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AttemptManager::NotifyJobOfFailure,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }

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

  allowed_bad_certs_ = allowed_bad_certs;
  quic_version_ = quic_version;

  StartInternal(priority);

  return;
}

int HttpStreamPool::AttemptManager::Preconnect(
    size_t num_streams,
    quic::ParsedQuicVersion quic_version,
    CompletionOnceCallback callback) {
  // HttpStreamPool should check the existing QUIC/SPDY sessions before calling
  // this method.
  CHECK(!CanUseExistingQuicSession());
  CHECK(!spdy_session_);
  CHECK(!spdy_session_pool()->HasAvailableSession(spdy_session_key(),
                                                  /*is_websocket=*/false));
  CHECK(group_->ActiveStreamSocketCount() < num_streams);

  if (is_failing_) {
    return error_to_notify_;
  }

  auto entry =
      std::make_unique<PreconnectEntry>(num_streams, std::move(callback));
  preconnects_.emplace(std::move(entry));

  quic_version_ = quic_version;

  StartInternal(RequestPriority::IDLE);
  return ERR_IO_PENDING;
}

void HttpStreamPool::AttemptManager::OnServiceEndpointsUpdated() {
  // For plain HTTP request, we need to wait for HTTPS RR because we could
  // trigger HTTP -> HTTPS upgrade when HTTPS RR is received during the endpoint
  // resolution.
  if (UsingTls() || service_endpoint_request_->EndpointsCryptoReady()) {
    ProcessServiceEndpointChanges();
  }
}

void HttpStreamPool::AttemptManager::OnServiceEndpointRequestFinished(int rv) {
  CHECK(!service_endpoint_request_finished_);
  CHECK(service_endpoint_request_);

  service_endpoint_request_finished_ = true;
  dns_resolution_end_time_ = base::TimeTicks::Now();
  resolve_error_info_ = service_endpoint_request_->GetResolveErrorInfo();

  if (rv != OK) {
    error_to_notify_ = rv;
    // If service endpoint resolution failed, record an empty endpoint and the
    // result.
    connection_attempts_.emplace_back(IPEndPoint(), rv);
    NotifyFailure();
    return;
  }

  CHECK(!service_endpoint_request_->GetEndpointResults().empty());
  ProcessServiceEndpointChanges();
}

int HttpStreamPool::AttemptManager::WaitForSSLConfigReady(
    CompletionOnceCallback callback) {
  if (ssl_config_.has_value()) {
    return OK;
  }

  ssl_config_waiting_callbacks_.emplace_back(std::move(callback));
  return ERR_IO_PENDING;
}

SSLConfig HttpStreamPool::AttemptManager::GetSSLConfig() {
  CHECK(ssl_config_.has_value());
  return *ssl_config_;
}

void HttpStreamPool::AttemptManager::ProcessPendingJob() {
  if (is_failing_) {
    return;
  }

  // Try to assign an idle stream to a job.
  if (jobs_.size() > 0 && group_->IdleStreamSocketCount() > 0) {
    std::unique_ptr<StreamSocket> stream_socket = group_->GetIdleStreamSocket();
    CHECK(stream_socket);
    const StreamSocketHandle::SocketReuseType reuse_type =
        GetReuseTypeFromIdleStreamSocket(*stream_socket);
    CreateTextBasedStreamAndNotify(std::move(stream_socket), reuse_type,
                                   LoadTimingInfo::ConnectTiming());
    return;
  }

  const size_t pending_job_count = PendingJobCount();
  const size_t pending_preconnect_count = PendingPreconnectCount();

  if (pending_job_count == 0 && pending_preconnect_count == 0) {
    return;
  }

  CHECK(!CanUseExistingQuicSession());
  CHECK(!spdy_session_);

  MaybeAttemptConnection(/*max_attempts=*/1);
}

void HttpStreamPool::AttemptManager::CancelInFlightAttempts() {
  pool()->DecrementTotalConnectingStreamCount(in_flight_attempts_.size());
  in_flight_attempts_.clear();
  slow_attempt_count_ = 0;
}

void HttpStreamPool::AttemptManager::OnJobComplete(Job* job) {
  auto notified_it = notified_jobs_.find(job);
  if (notified_it != notified_jobs_.end()) {
    notified_jobs_.erase(notified_it);
  } else {
    for (JobQueue::Pointer pointer = jobs_.FirstMax(); !pointer.is_null();
         pointer = jobs_.GetNextTowardsLastMin(pointer)) {
      if (pointer.value() == job) {
        jobs_.Erase(pointer);
        break;
      }
    }
  }
  MaybeComplete();
}

void HttpStreamPool::AttemptManager::CancelJobs(int error) {
  error_to_notify_ = error;
  is_canceling_jobs_ = true;
  NotifyFailure();
}

size_t HttpStreamPool::AttemptManager::PendingJobCount() const {
  return PendingCountInternal(jobs_.size());
}

size_t HttpStreamPool::AttemptManager::PendingPreconnectCount() const {
  size_t num_streams = 0;
  for (const auto& entry : preconnects_) {
    num_streams = std::max(num_streams, entry->num_streams);
  }
  return PendingCountInternal(num_streams);
}

const HttpStreamKey& HttpStreamPool::AttemptManager::stream_key() const {
  return group_->stream_key();
}

const SpdySessionKey& HttpStreamPool::AttemptManager::spdy_session_key() const {
  return group_->spdy_session_key();
}

const QuicSessionKey& HttpStreamPool::AttemptManager::quic_session_key() const {
  return group_->quic_session_key();
}

HttpNetworkSession* HttpStreamPool::AttemptManager::http_network_session() {
  return group_->http_network_session();
}

SpdySessionPool* HttpStreamPool::AttemptManager::spdy_session_pool() {
  return http_network_session()->spdy_session_pool();
}

QuicSessionPool* HttpStreamPool::AttemptManager::quic_session_pool() {
  return http_network_session()->quic_session_pool();
}

HttpStreamPool* HttpStreamPool::AttemptManager::pool() {
  return group_->pool();
}

const HttpStreamPool* HttpStreamPool::AttemptManager::pool() const {
  return group_->pool();
}

const NetLogWithSource& HttpStreamPool::AttemptManager::net_log() {
  return net_log_;
}

bool HttpStreamPool::AttemptManager::UsingTls() const {
  return GURL::SchemeIsCryptographic(stream_key().destination().scheme());
}

bool HttpStreamPool::AttemptManager::RequiresHTTP11() {
  return pool()->RequiresHTTP11(stream_key());
}

LoadState HttpStreamPool::AttemptManager::GetLoadState() const {
  if (group_->ReachedMaxStreamLimit()) {
    return LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET;
  }

  if (pool()->ReachedMaxStreamLimit()) {
    return LOAD_STATE_WAITING_FOR_STALLED_SOCKET_POOL;
  }

  LoadState load_state = LOAD_STATE_IDLE;

  // When there are in-flight attempts, use most advanced one.
  for (const auto& in_flight_attempt : in_flight_attempts_) {
    load_state =
        std::max(load_state, in_flight_attempt->attempt()->GetLoadState());
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
  if (jobs_.empty()) {
    CHECK(!preconnects_.empty());
    // Preconnets have IDLE priority.
    return RequestPriority::IDLE;
  }
  return static_cast<RequestPriority>(jobs_.FirstMax().priority());
}

bool HttpStreamPool::AttemptManager::IsStalledByPoolLimit() {
  if (is_failing_) {
    return false;
  }

  if (!GetIPEndPointToAttempt().has_value()) {
    return false;
  }

  if (CanUseExistingQuicSession() || spdy_session_) {
    CHECK_EQ(PendingPreconnectCount(), 0u);
    return false;
  }

  switch (CanAttemptConnection()) {
    case CanAttemptResult::kAttempt:
    case CanAttemptResult::kReachedPoolLimit:
      return true;
    case CanAttemptResult::kNoPendingJob:
    case CanAttemptResult::kBlockedStreamAttempt:
    case CanAttemptResult::kThrottledForSpdy:
    case CanAttemptResult::kReachedGroupLimit:
      return false;
  }
}

void HttpStreamPool::AttemptManager::OnRequiredHttp11() {
  if (spdy_session_) {
    spdy_session_.reset();
    is_failing_ = true;
    error_to_notify_ = ERR_HTTP_1_1_REQUIRED;
  }
}

void HttpStreamPool::AttemptManager::OnQuicTaskComplete(
    int rv,
    NetErrorDetails details) {
  CHECK(!quic_task_result_.has_value());
  quic_task_result_ = rv;
  net_error_details_ = std::move(details);
  quic_task_.reset();

  MaybeMarkQuicBroken();

  const bool has_jobs = !jobs_.empty() || !notified_jobs_.empty();

  if (rv == OK) {
    HandleQuicSessionReady();
    if (has_jobs) {
      CreateQuicStreamAndNotify();
      return;
    }
  }

  if (rv != OK &&
      (tcp_based_attempt_state_ == TcpBasedAttemptState::kAllAttemptsFailed ||
       group_->force_quic())) {
    error_to_notify_ = rv;
    NotifyFailure();
    return;
  }

  if (rv != OK || should_block_stream_attempt_) {
    should_block_stream_attempt_ = false;
    stream_attempt_delay_timer_.Stop();
    MaybeAttemptConnection();
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AttemptManager::MaybeComplete,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

base::Value::Dict HttpStreamPool::AttemptManager::GetInfoAsValue() {
  base::Value::Dict dict;
  dict.Set("pending_job_count", static_cast<int>(PendingJobCount()));
  dict.Set("pending_preconnect_count",
           static_cast<int>(PendingPreconnectCount()));
  dict.Set("is_stalled", IsStalledByPoolLimit());
  return dict;
}

void HttpStreamPool::AttemptManager::StartInternal(RequestPriority priority) {
  UpdateStreamAttemptState();

  if (service_endpoint_request_ || service_endpoint_request_finished_) {
    MaybeAttemptQuic();
    MaybeAttemptConnection();
  } else {
    ResolveServiceEndpoint(priority);
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

void HttpStreamPool::AttemptManager::
    MaybeChangeServiceEndpointRequestPriority() {
  if (service_endpoint_request_ && !service_endpoint_request_finished_) {
    service_endpoint_request_->ChangeRequestPriority(GetPriority());
  }
}

void HttpStreamPool::AttemptManager::ProcessServiceEndpointChanges() {
  if (CanUseExistingSessionAfterEndpointChanges()) {
    return;
  }
  MaybeRunStreamAttemptDelayTimer();
  MaybeCalculateSSLConfig();
  MaybeAttemptQuic();
  MaybeAttemptConnection();
}

bool HttpStreamPool::AttemptManager::
    CanUseExistingSessionAfterEndpointChanges() {
  CHECK(service_endpoint_request_);

  if (!UsingTls()) {
    return false;
  }

  if (CanUseExistingQuicSession()) {
    return true;
  }

  if (CanUseQuic()) {
    QuicSessionAliasKey quic_session_alias_key(stream_key().destination(),
                                               quic_session_key());
    for (const auto& endpoint :
         service_endpoint_request_->GetEndpointResults()) {
      if (quic_session_pool()->HasMatchingIpSessionForServiceEndpoint(
              quic_session_alias_key, endpoint,
              service_endpoint_request_->GetDnsAliasResults(), true)) {
        if (quic_task_) {
          quic_task_result_ = OK;
          quic_task_.reset();
        }
        HandleQuicSessionReady();
        // Use PostTask() because we could reach here from RequestStream()
        // synchronously when the DNS resolution finishes immediately.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(&AttemptManager::CreateQuicStreamAndNotify,
                           weak_ptr_factory_.GetWeakPtr()));
        return true;
      }
    }
  }

  if (spdy_session_) {
    return true;
  }

  if (!enable_ip_based_pooling_) {
    return false;
  }

  for (const auto& endpoint : service_endpoint_request_->GetEndpointResults()) {
    spdy_session_ =
        spdy_session_pool()->FindMatchingIpSessionForServiceEndpoint(
            spdy_session_key(), endpoint,
            service_endpoint_request_->GetDnsAliasResults());
    if (spdy_session_) {
      HandleSpdySessionReady();
      // Use PostTask() because we could reach here from RequestStream()
      // synchronously when the DNS resolution finishes immediately.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&AttemptManager::CreateSpdyStreamAndNotify,
                                    weak_ptr_factory_.GetWeakPtr()));
      return true;
    }
  }

  return false;
}

void HttpStreamPool::AttemptManager::MaybeRunStreamAttemptDelayTimer() {
  if (!should_block_stream_attempt_ ||
      stream_attempt_delay_timer_.IsRunning()) {
    return;
  }
  CHECK(!stream_attempt_delay_.is_zero());
  stream_attempt_delay_timer_.Start(
      FROM_HERE, stream_attempt_delay_,
      base::BindOnce(&AttemptManager::OnStreamAttemptDelayPassed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HttpStreamPool::AttemptManager::MaybeCalculateSSLConfig() {
  if (!UsingTls() || ssl_config_.has_value()) {
    return;
  }

  CHECK(service_endpoint_request_);
  if (!service_endpoint_request_->EndpointsCryptoReady()) {
    return;
  }

  SSLConfig ssl_config;

  ssl_config.allowed_bad_certs = allowed_bad_certs_;
  ssl_config.privacy_mode = stream_key().privacy_mode();
  ssl_config.disable_cert_verification_network_fetches =
      stream_key().disable_cert_network_fetches();
  ssl_config.early_data_enabled =
      http_network_session()->params().enable_early_data;

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

  // TODO(crbug.com/346835898): Support ECH.

  ssl_config_.emplace(std::move(ssl_config));

  // Restart slow timer for in-flight attempts that have already completed
  // TCP handshakes.
  for (auto& in_flight_attempt : in_flight_attempts_) {
    if (!in_flight_attempt->is_slow() &&
        !in_flight_attempt->slow_timer().IsRunning()) {
      // TODO(crbug.com/346835898): Should we use a different delay other than
      // the connection attempt delay?
      // base::Unretained() is safe here because `this` owns the
      // `in_flight_attempt` and `slow_timer`.
      in_flight_attempt->slow_timer().Start(
          FROM_HERE, kConnectionAttemptDelay,
          base::BindOnce(&AttemptManager::OnInFlightAttemptSlow,
                         base::Unretained(this), in_flight_attempt.get()));
    }
  }

  for (auto& callback : ssl_config_waiting_callbacks_) {
    std::move(callback).Run(OK);
  }
  ssl_config_waiting_callbacks_.clear();
}

void HttpStreamPool::AttemptManager::MaybeAttemptQuic() {
  CHECK(service_endpoint_request_);
  if (!CanUseQuic() || quic_task_result_.has_value() ||
      !service_endpoint_request_->EndpointsCryptoReady()) {
    return;
  }

  if (!quic_task_) {
    quic_task_ = std::make_unique<QuicTask>(this, quic_version_);
  }
  quic_task_->MaybeAttempt();
}

void HttpStreamPool::AttemptManager::MaybeAttemptConnection(
    std::optional<size_t> max_attempts) {
  if (PendingJobCount() == 0 && preconnects_.empty()) {
    // There are no jobs waiting for streams.
    return;
  }

  if (group_->force_quic()) {
    return;
  }

  if (CanUseQuic() && quic_task_result_.has_value() &&
      *quic_task_result_ == OK) {
    return;
  }

  CHECK(!preconnects_.empty() || group_->IdleStreamSocketCount() == 0);

  // TODO(crbug.com/346835898): Ensure that we don't attempt connections when
  // failing or creating HttpStream on top of a SPDY session.
  CHECK(!is_failing_);
  CHECK(!spdy_session_);

  std::optional<IPEndPoint> ip_endpoint = GetIPEndPointToAttempt();
  if (!ip_endpoint.has_value()) {
    if (service_endpoint_request_finished_ && in_flight_attempts_.empty()) {
      tcp_based_attempt_state_ = TcpBasedAttemptState::kAllAttemptsFailed;
    }
    if (tcp_based_attempt_state_ == TcpBasedAttemptState::kAllAttemptsFailed &&
        !quic_task_) {
      // Tried all endpoints.
      MaybeMarkQuicBroken();
      NotifyFailure();
    }
    return;
  }

  if (tcp_based_attempt_state_ == TcpBasedAttemptState::kNotStarted) {
    tcp_based_attempt_state_ = TcpBasedAttemptState::kAttempting;
  }

  // There might be multiple pending jobs. Make attempts as much as needed
  // and allowed.
  size_t num_attempts = 0;
  const bool using_tls = UsingTls();
  while (IsConnectionAttemptReady()) {
    auto in_flight_attempt = std::make_unique<InFlightAttempt>(this);
    InFlightAttempt* raw_attempt = in_flight_attempt.get();
    in_flight_attempts_.emplace(std::move(in_flight_attempt));
    pool()->IncrementTotalConnectingStreamCount();

    std::unique_ptr<StreamAttempt> attempt;
    // Set to non-null if the attempt is a TLS attempt.
    TlsStreamAttempt* tls_attempt_ptr = nullptr;
    if (using_tls) {
      attempt = std::make_unique<TlsStreamAttempt>(
          pool()->stream_attempt_params(), *ip_endpoint,
          HostPortPair::FromSchemeHostPort(stream_key().destination()),
          /*ssl_config_provider=*/raw_attempt);
      tls_attempt_ptr = static_cast<TlsStreamAttempt*>(attempt.get());
    } else {
      attempt = std::make_unique<TcpStreamAttempt>(
          pool()->stream_attempt_params(), *ip_endpoint);
    }

    net_log().AddEventReferencingSource(
        NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_ATTEMPT_START,
        attempt->net_log().source());
    net_log().AddEvent(
        NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_ATTEMPT_START, [&] {
          base::Value::Dict dict;
          dict.Set("num_jobs", static_cast<int>(jobs_.size()));
          dict.Set("num_preconnects", static_cast<int>(preconnects_.size()));
          dict.Set("num_inflight_attempts",
                   static_cast<int>(in_flight_attempts_.size()));
          dict.Set("num_slow_attempts", static_cast<int>(slow_attempt_count_));
          attempt->net_log().source().AddToEventParameters(dict);
          return dict;
        });

    int rv = raw_attempt->Start(std::move(attempt));
    // Add NetLog dependency after Start() so that the first event of the
    // attempt can have meaningful description in the NetLog viewer.
    raw_attempt->attempt()->net_log().AddEventReferencingSource(
        NetLogEventType::STREAM_ATTEMPT_BOUND_TO_POOL, net_log().source());
    if (rv != ERR_IO_PENDING) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&AttemptManager::OnInFlightAttemptComplete,
                                    base::Unretained(this), raw_attempt, rv));
    } else {
      raw_attempt->slow_timer().Start(
          FROM_HERE, kConnectionAttemptDelay,
          base::BindOnce(&AttemptManager::OnInFlightAttemptSlow,
                         base::Unretained(this), raw_attempt));
      if (tls_attempt_ptr && !tls_attempt_ptr->IsTcpHandshakeCompleted()) {
        tls_attempt_ptr->SetTcpHandshakeCompletionCallback(base::BindOnce(
            &AttemptManager::OnInFlightAttemptTcpHandshakeComplete,
            base::Unretained(this), raw_attempt));
      }
    }

    ++num_attempts;
    if (max_attempts.has_value() && num_attempts >= *max_attempts) {
      break;
    }
  }
}

bool HttpStreamPool::AttemptManager::IsConnectionAttemptReady() {
  switch (CanAttemptConnection()) {
    case CanAttemptResult::kAttempt:
      return true;
    case CanAttemptResult::kNoPendingJob:
      return false;
    case CanAttemptResult::kBlockedStreamAttempt:
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
HttpStreamPool::AttemptManager::CanAttemptConnection() {
  size_t pending_count = std::max(PendingJobCount(), PendingPreconnectCount());
  if (pending_count == 0) {
    return CanAttemptResult::kNoPendingJob;
  }

  if (ShouldThrottleAttemptForSpdy()) {
    return CanAttemptResult::kThrottledForSpdy;
  }

  if (should_block_stream_attempt_) {
    return CanAttemptResult::kBlockedStreamAttempt;
  }

  if (respect_limits_ == RespectLimits::kRespect) {
    if (group_->ReachedMaxStreamLimit()) {
      return CanAttemptResult::kReachedGroupLimit;
    }

    if (pool()->ReachedMaxStreamLimit()) {
      return CanAttemptResult::kReachedPoolLimit;
    }
  }

  return CanAttemptResult::kAttempt;
}

bool HttpStreamPool::AttemptManager::ShouldThrottleAttemptForSpdy() {
  if (!http_network_session()->http_server_properties()->GetSupportsSpdy(
          stream_key().destination(),
          stream_key().network_anonymization_key())) {
    return false;
  }

  CHECK(UsingTls());

  // The first attempt should not be blocked.
  if (in_flight_attempts_.empty()) {
    return false;
  }

  if (spdy_throttle_delay_passed_) {
    return false;
  }

  CHECK(!spdy_session_);
  return true;
}

size_t HttpStreamPool::AttemptManager::PendingCountInternal(
    size_t pending_count) const {
  CHECK_GE(in_flight_attempts_.size(), slow_attempt_count_);
  // When SPDY throttle delay passed, treat all in-flight attempts as non-slow,
  // to avoid attempting connections more than requested.
  // TODO(crbug.com/346835898): This behavior is tricky. Figure out a better
  // way to handle this situation.
  size_t slow_count = spdy_throttle_delay_passed_ ? 0 : slow_attempt_count_;
  size_t non_slow_count = in_flight_attempts_.size() - slow_count;
  // The number of in-flight, non-slow attempts could be larger than the number
  // of jobs (e.g. a job was cancelled in the middle of an attempt).
  if (pending_count <= non_slow_count) {
    return 0;
  }

  return pending_count - non_slow_count;
}

std::optional<IPEndPoint>
HttpStreamPool::AttemptManager::GetIPEndPointToAttempt() {
  if (!service_endpoint_request_ ||
      service_endpoint_request_->GetEndpointResults().empty()) {
    return std::nullopt;
  }

  // Look for an IPEndPoint from the preferred address family first.
  for (auto& endpoint : service_endpoint_request_->GetEndpointResults()) {
    std::optional<IPEndPoint> ip_endpoint =
        prefer_ipv6_ ? FindPreferredIPEndpoint(endpoint.ipv6_endpoints)
                     : FindPreferredIPEndpoint(endpoint.ipv4_endpoints);
    if (ip_endpoint.has_value()) {
      return ip_endpoint;
    }
  }

  // If there is no IPEndPoint from the preferred address family, check the
  // another address family.
  for (auto& endpoint : service_endpoint_request_->GetEndpointResults()) {
    std::optional<IPEndPoint> ip_endpoint =
        prefer_ipv6_ ? FindPreferredIPEndpoint(endpoint.ipv4_endpoints)
                     : FindPreferredIPEndpoint(endpoint.ipv6_endpoints);
    if (ip_endpoint.has_value()) {
      return ip_endpoint;
    }
  }

  return std::nullopt;
}

std::optional<IPEndPoint>
HttpStreamPool::AttemptManager::FindPreferredIPEndpoint(
    const std::vector<IPEndPoint>& ip_endpoints) {
  // Prefer the first unattempted endpoint in `ip_endpoints`. Allow to use
  // the first slow endpoint when SPDY throttle delay passed.

  std::optional<IPEndPoint> slow_endpoint;
  for (const auto& ip_endpoint : ip_endpoints) {
    if (base::Contains(failed_ip_endpoints_, ip_endpoint)) {
      continue;
    }
    if (base::Contains(slow_ip_endpoints_, ip_endpoint)) {
      if (!slow_endpoint.has_value()) {
        slow_endpoint = ip_endpoint;
      }
      continue;
    }
    return ip_endpoint;
  }

  if (spdy_throttle_delay_passed_) {
    return slow_endpoint;
  }
  return std::nullopt;
}

HttpStreamPool::AttemptManager::FailureKind
HttpStreamPool::AttemptManager::DetermineFailureKind() {
  if (is_canceling_jobs_) {
    return FailureKind::kStreamFailed;
  }

  if (IsCertificateError(error_to_notify_)) {
    return FailureKind::kCertifcateError;
  }

  if (error_to_notify_ == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    return FailureKind::kNeedsClientAuth;
  }

  return FailureKind::kStreamFailed;
}

void HttpStreamPool::AttemptManager::NotifyFailure() {
  is_failing_ = true;
  NotifyPreconnectsComplete(error_to_notify_);
  NotifyJobOfFailure();
  // `this` may be deleted.
}

void HttpStreamPool::AttemptManager::NotifyJobOfFailure() {
  CHECK(is_failing_);
  Job* job = ExtractFirstJobToNotify();
  if (!job) {
    // TODO(crbug.com/346835898): Ensure that MaybeComplete() is called
    // eventually.
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AttemptManager::NotifyJobOfFailure,
                                weak_ptr_factory_.GetWeakPtr()));

  job->AddConnectionAttempts(connection_attempts_);

  FailureKind kind = DetermineFailureKind();
  switch (kind) {
    case FailureKind::kStreamFailed:
      job->OnStreamFailed(error_to_notify_, net_error_details_,
                          resolve_error_info_);
      break;
    case FailureKind::kCertifcateError:
      CHECK(cert_error_ssl_info_.has_value());
      job->OnCertificateError(error_to_notify_, *cert_error_ssl_info_);
      break;
    case FailureKind::kNeedsClientAuth:
      CHECK(client_auth_cert_info_.get());
      job->OnNeedsClientAuth(client_auth_cert_info_.get());
      break;
  }
  // `this` may be deleted.
}

void HttpStreamPool::AttemptManager::NotifyPreconnectsComplete(int rv) {
  while (!preconnects_.empty()) {
    std::unique_ptr<PreconnectEntry> entry =
        std::move(preconnects_.extract(preconnects_.begin()).value());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(entry->callback), rv));
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AttemptManager::MaybeComplete,
                                weak_ptr_factory_.GetWeakPtr()));
}

void HttpStreamPool::AttemptManager::ProcessPreconnectsAfterAttemptComplete(
    int rv) {
  std::vector<PreconnectEntry*> completed;
  for (auto& entry : preconnects_) {
    CHECK_GT(entry->num_streams, 0u);
    --entry->num_streams;
    if (rv != OK) {
      entry->result = rv;
    }
    if (entry->num_streams == 0) {
      completed.emplace_back(entry.get());
    }
  }

  for (auto* entry_ptr : completed) {
    auto it = preconnects_.find(entry_ptr);
    CHECK(it != preconnects_.end());
    std::unique_ptr<PreconnectEntry> entry =
        std::move(preconnects_.extract(it).value());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(entry->callback), entry->result));
  }
  if (preconnects_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AttemptManager::MaybeComplete,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void HttpStreamPool::AttemptManager::CreateTextBasedStreamAndNotify(
    std::unique_ptr<StreamSocket> stream_socket,
    StreamSocketHandle::SocketReuseType reuse_type,
    LoadTimingInfo::ConnectTiming connect_timing) {
  NextProto negotiated_protocol = stream_socket->GetNegotiatedProtocol();
  CHECK_NE(negotiated_protocol, NextProto::kProtoHTTP2);

  std::unique_ptr<HttpStream> http_stream = group_->CreateTextBasedStream(
      std::move(stream_socket), reuse_type, std::move(connect_timing));
  CHECK(respect_limits_ == RespectLimits::kIgnore ||
        group_->ActiveStreamSocketCount() <=
            pool()->max_stream_sockets_per_group())
      << "active=" << group_->ActiveStreamSocketCount()
      << ", limit=" << pool()->max_stream_sockets_per_group();

  NotifyStreamReady(std::move(http_stream), negotiated_protocol);
  // `this` may be deleted.
}

void HttpStreamPool::AttemptManager::CreateSpdyStreamAndNotify() {
  CHECK(!is_canceling_jobs_);
  CHECK(!is_failing_);

  if (!spdy_session_ || !spdy_session_->IsAvailable()) {
    // There was an available SPDY session but the session has gone while
    // notifying to jobs. Do another attempt.

    spdy_session_.reset();
    // We may not have calculated SSLConfig yet. Try to calculate it before
    // attempting connections.
    MaybeCalculateSSLConfig();
    MaybeAttemptConnection();
    return;
  }

  // If there are more than one remaining job, post a task to create
  // HttpStreams for these jobs.
  if (jobs_.size() > 1) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AttemptManager::CreateSpdyStreamAndNotify,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  std::set<std::string> dns_aliases =
      http_network_session()->spdy_session_pool()->GetDnsAliasesForSessionKey(
          spdy_session_key());
  auto http_stream = std::make_unique<SpdyHttpStream>(
      spdy_session_, net_log().source(), std::move(dns_aliases));
  NotifyStreamReady(std::move(http_stream), NextProto::kProtoHTTP2);
  // `this` may be deleted.
}

void HttpStreamPool::AttemptManager::CreateQuicStreamAndNotify() {
  QuicChromiumClientSession* quic_session =
      quic_session_pool()->FindExistingSession(quic_session_key(),
                                               stream_key().destination());
  CHECK(quic_session);

  // If there are more than one remaining job, post a task to create
  // HttpStreams for these jobs.
  if (jobs_.size() > 1) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AttemptManager::CreateQuicStreamAndNotify,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  std::set<std::string> dns_aliases =
      quic_session->GetDnsAliasesForSessionKey(quic_session_key());
  auto http_stream = std::make_unique<QuicHttpStream>(
      quic_session->CreateHandle(stream_key().destination()),
      std::move(dns_aliases));
  NotifyStreamReady(std::move(http_stream), NextProto::kProtoQUIC);
  // `this` may be deleted.
}

void HttpStreamPool::AttemptManager::NotifyStreamReady(
    std::unique_ptr<HttpStream> stream,
    NextProto negotiated_protocol) {
  Job* job = ExtractFirstJobToNotify();
  if (!job) {
    // The ownership of the stream will be moved to the group as `stream` is
    // going to be destructed.
    return;
  }
  job->OnStreamReady(std::move(stream), negotiated_protocol);
}

void HttpStreamPool::AttemptManager::HandleSpdySessionReady() {
  CHECK(!group_->force_quic());
  CHECK(!is_failing_);
  CHECK(spdy_session_);

  group_->Refresh(kSwitchingToHttp2);
  NotifyPreconnectsComplete(OK);
}

void HttpStreamPool::AttemptManager::HandleQuicSessionReady() {
  CHECK(!is_failing_);
  CHECK(!quic_task_);
  DCHECK(CanUseExistingQuicSession());

  group_->Refresh(kSwitchingToHttp3);
  NotifyPreconnectsComplete(OK);
}

HttpStreamPool::Job* HttpStreamPool::AttemptManager::ExtractFirstJobToNotify() {
  if (jobs_.empty()) {
    return nullptr;
  }
  raw_ptr<Job> job = jobs_.Erase(jobs_.FirstMax());
  Job* job_raw_ptr = job.get();
  notified_jobs_.emplace(std::move(job));
  return job_raw_ptr;
}

void HttpStreamPool::AttemptManager::SetJobPriority(Job* job,
                                                    RequestPriority priority) {
  for (JobQueue::Pointer pointer = jobs_.FirstMax(); !pointer.is_null();
       pointer = jobs_.GetNextTowardsLastMin(pointer)) {
    if (pointer.value() == job) {
      if (pointer.priority() == priority) {
        break;
      }

      raw_ptr<Job> entry = jobs_.Erase(pointer);
      jobs_.Insert(std::move(entry), priority);
      break;
    }
  }

  MaybeChangeServiceEndpointRequestPriority();
}

void HttpStreamPool::AttemptManager::OnInFlightAttemptComplete(
    InFlightAttempt* raw_attempt,
    int rv) {
  net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_ATTEMPT_END,
      raw_attempt->attempt()->net_log().source());
  raw_attempt->slow_timer().Stop();
  if (raw_attempt->is_slow()) {
    CHECK_GT(slow_attempt_count_, 0u);
    --slow_attempt_count_;
  }

  auto it = in_flight_attempts_.find(raw_attempt);
  CHECK(it != in_flight_attempts_.end());
  std::unique_ptr<InFlightAttempt> in_flight_attempt =
      std::move(in_flight_attempts_.extract(it).value());
  pool()->DecrementTotalConnectingStreamCount();

  if (rv != OK) {
    connection_attempts_.emplace_back(
        in_flight_attempt->attempt()->ip_endpoint(), rv);
    HandleAttemptFailure(std::move(in_flight_attempt), rv);
    return;
  }

  CHECK_NE(tcp_based_attempt_state_, TcpBasedAttemptState::kAllAttemptsFailed);
  if (tcp_based_attempt_state_ == TcpBasedAttemptState::kAttempting) {
    tcp_based_attempt_state_ = TcpBasedAttemptState::kSucceededAtLeastOnce;
    MaybeMarkQuicBroken();
  }

  LoadTimingInfo::ConnectTiming connect_timing =
      in_flight_attempt->attempt()->connect_timing();
  connect_timing.domain_lookup_start = dns_resolution_start_time_;
  // If the attempt started before DNS resolution completion, `connect_start`
  // could be smaller than `dns_resolution_end_time_`. Use the smallest one.
  connect_timing.domain_lookup_end =
      dns_resolution_end_time_.is_null()
          ? connect_timing.connect_start
          : std::min(connect_timing.connect_start, dns_resolution_end_time_);

  std::unique_ptr<StreamSocket> stream_socket =
      in_flight_attempt->attempt()->ReleaseStreamSocket();
  CHECK(stream_socket);
  CHECK(service_endpoint_request_);
  stream_socket->SetDnsAliases(service_endpoint_request_->GetDnsAliasResults());

  spdy_throttle_timer_.Stop();

  const auto reuse_type = StreamSocketHandle::SocketReuseType::kUnused;
  if (stream_socket->GetNegotiatedProtocol() == NextProto::kProtoHTTP2) {
    CHECK(!spdy_session_pool()->FindAvailableSession(
        group_->spdy_session_key(), enable_ip_based_pooling_,
        /*is_websocket=*/false, net_log()));
    std::unique_ptr<HttpStreamPoolHandle> handle = group_->CreateHandle(
        std::move(stream_socket), reuse_type, std::move(connect_timing));
    int create_result =
        spdy_session_pool()->CreateAvailableSessionFromSocketHandle(
            spdy_session_key(), std::move(handle), net_log(), &spdy_session_);
    if (create_result != OK) {
      HandleAttemptFailure(std::move(in_flight_attempt), create_result);
      return;
    }

    HandleSpdySessionReady();
    CreateSpdyStreamAndNotify();
    return;
  }

  ProcessPreconnectsAfterAttemptComplete(rv);

  CHECK_NE(stream_socket->GetNegotiatedProtocol(), NextProto::kProtoHTTP2);
  CreateTextBasedStreamAndNotify(std::move(stream_socket), reuse_type,
                                 std::move(connect_timing));
}

void HttpStreamPool::AttemptManager::OnInFlightAttemptTcpHandshakeComplete(
    InFlightAttempt* raw_attempt,
    int rv) {
  auto it = in_flight_attempts_.find(raw_attempt);
  CHECK(it != in_flight_attempts_.end());
  if (raw_attempt->is_slow() || !raw_attempt->slow_timer().IsRunning()) {
    return;
  }

  raw_attempt->slow_timer().Stop();
}

void HttpStreamPool::AttemptManager::OnInFlightAttemptSlow(
    InFlightAttempt* raw_attempt) {
  auto it = in_flight_attempts_.find(raw_attempt);
  CHECK(it != in_flight_attempts_.end());

  raw_attempt->set_is_slow(true);
  ++slow_attempt_count_;
  slow_ip_endpoints_.emplace(raw_attempt->attempt()->ip_endpoint());
  prefer_ipv6_ = !raw_attempt->attempt()->ip_endpoint().address().IsIPv6();

  MaybeAttemptConnection();
}

void HttpStreamPool::AttemptManager::HandleAttemptFailure(
    std::unique_ptr<InFlightAttempt> in_flight_attempt,
    int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  failed_ip_endpoints_.emplace(in_flight_attempt->attempt()->ip_endpoint());

  ProcessPreconnectsAfterAttemptComplete(rv);

  if (is_failing_) {
    // `this` has already failed and is notifying jobs to the failure.
    return;
  }

  error_to_notify_ = rv;

  if (rv == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    CHECK(UsingTls());
    client_auth_cert_info_ = in_flight_attempt->attempt()->GetCertRequestInfo();
    in_flight_attempt.reset();
    NotifyFailure();
    return;
  }

  if (IsCertificateError(rv)) {
    // When a certificate error happened for an attempt, notifies all jobs of
    // the error.
    CHECK(UsingTls());
    CHECK(in_flight_attempt->attempt()->stream_socket());
    SSLInfo ssl_info;
    bool has_ssl_info =
        in_flight_attempt->attempt()->stream_socket()->GetSSLInfo(&ssl_info);
    CHECK(has_ssl_info);
    cert_error_ssl_info_ = ssl_info;
    in_flight_attempt.reset();
    NotifyFailure();
  } else {
    in_flight_attempt.reset();
    MaybeAttemptConnection();
  }
}

void HttpStreamPool::AttemptManager::OnSpdyThrottleDelayPassed() {
  CHECK(!spdy_throttle_delay_passed_);
  spdy_throttle_delay_passed_ = true;
  MaybeAttemptConnection();
}

base::TimeDelta HttpStreamPool::AttemptManager::GetStreamAttemptDelay() {
  if (!CanUseQuic()) {
    return base::TimeDelta();
  }

  return quic_session_pool()->GetTimeDelayForWaitingJob(quic_session_key());
}

void HttpStreamPool::AttemptManager::UpdateStreamAttemptState() {
  if (!should_block_stream_attempt_) {
    return;
  }

  if (!CanUseQuic()) {
    should_block_stream_attempt_ = false;
    stream_attempt_delay_timer_.Stop();
    return;
  }
}

void HttpStreamPool::AttemptManager::OnStreamAttemptDelayPassed() {
  net_log().AddEvent(
      NetLogEventType::
          HTTP_STREAM_POOL_ATTEMPT_MANAGER_STREAM_ATTEMPT_DELAY_PASSED,
      [&] {
        base::Value::Dict dict;
        dict.Set("stream_attempt_delay",
                 static_cast<int>(stream_attempt_delay_.InMilliseconds()));
        return dict;
      });
  CHECK(should_block_stream_attempt_);
  should_block_stream_attempt_ = false;
  MaybeAttemptConnection();
}

bool HttpStreamPool::AttemptManager::CanUseQuic() {
  return pool()->CanUseQuic(stream_key(), enable_ip_based_pooling_,
                            enable_alternative_services_);
}

bool HttpStreamPool::AttemptManager::CanUseExistingQuicSession() {
  return pool()->CanUseExistingQuicSession(stream_key(), quic_session_key(),
                                           enable_ip_based_pooling_,
                                           enable_alternative_services_);
}

void HttpStreamPool::AttemptManager::MaybeMarkQuicBroken() {
  if (!quic_task_result_.has_value() ||
      tcp_based_attempt_state_ == TcpBasedAttemptState::kAttempting) {
    return;
  }

  if (*quic_task_result_ == OK ||
      *quic_task_result_ == ERR_DNS_NO_MATCHING_SUPPORTED_ALPN ||
      *quic_task_result_ == ERR_NETWORK_CHANGED ||
      *quic_task_result_ == ERR_INTERNET_DISCONNECTED) {
    return;
  }

  // No brokenness to report if we didn't attempt TCP-based connection or all
  // TCP-based attempts failed.
  if (tcp_based_attempt_state_ == TcpBasedAttemptState::kNotStarted ||
      tcp_based_attempt_state_ == TcpBasedAttemptState::kAllAttemptsFailed) {
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

void HttpStreamPool::AttemptManager::MaybeComplete() {
  if (!jobs_.empty() || !notified_jobs_.empty() || !preconnects_.empty() ||
      !in_flight_attempts_.empty()) {
    return;
  }

  if (quic_task_) {
    return;
  }

  group_->OnAttemptManagerComplete();
  // `this` is deleted.
}

}  // namespace net

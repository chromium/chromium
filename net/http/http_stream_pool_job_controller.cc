// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_job_controller.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/load_flags.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_internal_info.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/base/request_priority.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/alternative_service.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_group.h"
#include "net/http/http_stream_pool_job.h"
#include "net/http/http_stream_pool_request_info.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_util.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {

HttpStreamPool::JobController::Alternative::Alternative(
    HttpStreamKey stream_key,
    NextProto protocol,
    quic::ParsedQuicVersion quic_version,
    std::optional<QuicSessionAliasKey> quic_key)
    : stream_key(std::move(stream_key)),
      protocol(protocol),
      quic_version(quic_version),
      quic_key(std::move(quic_key)) {}

HttpStreamPool::JobController::Alternative::~Alternative() = default;

HttpStreamPool::JobController::Alternative::Alternative(Alternative&&) =
    default;

HttpStreamPool::JobController::Alternative&
HttpStreamPool::JobController::Alternative::operator=(Alternative&&) = default;

HttpStreamPool::JobController::PendingStream::PendingStream(
    std::unique_ptr<HttpStream> stream,
    NextProto negotiated_protocol,
    std::optional<SessionSource> session_source)
    : stream(std::move(stream)),
      negotiated_protocol(negotiated_protocol),
      session_source(session_source) {}

HttpStreamPool::JobController::PendingStream::PendingStream(PendingStream&&) =
    default;

HttpStreamPool::JobController::PendingStream::~PendingStream() = default;

HttpStreamPool::JobController::PendingStream&
HttpStreamPool::JobController::PendingStream::operator=(PendingStream&&) =
    default;

// static
std::optional<HttpStreamPool::JobController::Alternative>
HttpStreamPool::JobController::CalculateAlternative(
    HttpStreamPool* pool,
    const HttpStreamKey& origin_stream_key,
    const HttpStreamPoolRequestInfo& request_info,
    bool enable_alternative_services) {
  const NextProto protocol = request_info.alternative_service_info.protocol();

  if (!enable_alternative_services || protocol == NextProto::kProtoUnknown) {
    return std::nullopt;
  }

  CHECK(protocol == NextProto::kProtoHTTP2 ||
        protocol == NextProto::kProtoQUIC);

  url::SchemeHostPort destination(
      url::kHttpsScheme,
      request_info.alternative_service_info.GetHostPortPair().host(),
      request_info.alternative_service_info.GetHostPortPair().port());

  // If the alternative endpoint's destination is the same as origin, we don't
  // need an alternative job since the origin job will handle all protocols for
  // the destination.
  if (destination == request_info.destination) {
    return std::nullopt;
  }

  // If the alternative isn't QUIC but the destination is forced to use QUIC,
  // we shouldn't try the alternative.
  if (protocol != NextProto::kProtoQUIC &&
      pool->http_network_session()->ShouldForceQuic(
          destination, ProxyInfo::Direct(), /*is_websocket=*/false)) {
    return std::nullopt;
  }

  HttpStreamKey stream_key(
      destination, request_info.privacy_mode, request_info.socket_tag,
      request_info.network_anonymization_key, request_info.secure_dns_policy,
      request_info.disable_cert_network_fetches);

  quic::ParsedQuicVersion quic_version = quic::ParsedQuicVersion::Unsupported();
  std::optional<QuicSessionAliasKey> quic_key;
  if (protocol == NextProto::kProtoQUIC) {
    quic_version =
        pool->SelectQuicVersion(request_info.alternative_service_info);
    quic_key =
        origin_stream_key.CalculateQuicSessionAliasKey(std::move(destination));
  }

  return Alternative(std::move(stream_key), protocol, quic_version,
                     std::move(quic_key));
}

HttpStreamPool::JobController::JobController(
    HttpStreamPool* pool,
    HttpStreamPoolRequestInfo request_info,
    RequestPriority priority,
    std::vector<SSLConfig::CertAndStatus> allowed_bad_certs,
    bool enable_ip_based_pooling_for_h2,
    bool enable_alternative_services)
    : pool_(pool),
      priority_(priority),
      allowed_bad_certs_(std::move(allowed_bad_certs)),
      enable_ip_based_pooling_for_h2_(enable_ip_based_pooling_for_h2),
      enable_alternative_services_(enable_alternative_services),
      respect_limits_(request_info.load_flags & LOAD_IGNORE_LIMITS
                          ? RespectLimits::kIgnore
                          : RespectLimits::kRespect),
      allowed_alpns_(request_info.allowed_alpns),
      proxy_info_(request_info.proxy_info),
      alternative_service_info_(request_info.alternative_service_info),
      advertised_alt_svc_state_(request_info.advertised_alt_svc_state),
      origin_stream_key_(request_info.destination,
                         request_info.privacy_mode,
                         request_info.socket_tag,
                         request_info.network_anonymization_key,
                         request_info.secure_dns_policy,
                         request_info.disable_cert_network_fetches),
      origin_quic_key_(origin_stream_key_.CalculateQuicSessionAliasKey()),
      alternative_(CalculateAlternative(pool,
                                        origin_stream_key_,
                                        request_info,
                                        enable_alternative_services_)),
      net_log_(request_info.factory_job_controller_net_log),
      flow_(NetLogWithSourceToFlow(net_log_)),
      created_time_(base::TimeTicks::Now()) {
  TRACE_EVENT("net.stream", "JobController::JobController", flow_,
              "destination", request_info.destination.Serialize());
  net_log_.BeginEvent(
      NetLogEventType::HTTP_STREAM_POOL_JOB_CONTROLLER_ALIVE, [&] {
        base::Value::Dict dict;
        dict.Set("origin_destination",
                 origin_stream_key_.destination().Serialize());
        if (alternative_.has_value()) {
          dict.Set("alternative_destination",
                   alternative_->stream_key.destination().Serialize());
        }
        dict.Set("enable_ip_based_pooling_for_h2",
                 enable_ip_based_pooling_for_h2_);
        dict.Set("enable_alternative_services", enable_alternative_services_);
        dict.Set("respect_limits", respect_limits_ == RespectLimits::kRespect);
        return dict;
      });

  CHECK(proxy_info_.is_direct());
  if (!alternative_.has_value() &&
      alternative_service_info_.protocol() == NextProto::kProtoQUIC) {
    origin_quic_version_ = pool_->SelectQuicVersion(alternative_service_info_);
  }
}

HttpStreamPool::JobController::~JobController() {
  net_log_.EndEvent(NetLogEventType::HTTP_STREAM_POOL_JOB_CONTROLLER_ALIVE);
  TRACE_EVENT("net.stream", "JobController::~JobController", flow_);
}

void HttpStreamPool::JobController::HandleStreamRequest(
    HttpStreamRequest* stream_request,
    HttpStreamRequest::Delegate* delegate) {
  CHECK(stream_request);
  CHECK(!delegate_);
  CHECK(!stream_request_);
  TRACE_EVENT("net.stream", "JobController::HandleStreamRequest", flow_);

  stream_request->SetHelperForSwitchingToPool(this);
  delegate_ = delegate;
  stream_request_ = stream_request;

  if (pool_->delegate_for_testing_) {
    pool_->delegate_for_testing_->OnRequestStream(origin_stream_key_);
  }

  if (!IsPortAllowedForScheme(origin_stream_key_.destination().port(),
                              origin_stream_key_.destination().scheme())) {
    TaskRunner(priority_)->PostTask(
        FROM_HERE,
        base::BindOnce(&HttpStreamPool::JobController::CallOnStreamFailed,
                       weak_ptr_factory_.GetWeakPtr(), ERR_UNSAFE_PORT,
                       NetErrorDetails(), ResolveErrorInfo()));
    return;
  }

  pending_stream_ = MaybeCreateStreamFromExistingSession();
  if (pending_stream_) {
    TRACE_EVENT("net.stream", "JobController::CreateStreamFromExistingSession",
                "negotiated_protocol", pending_stream_->negotiated_protocol);

    if (pending_stream_->negotiated_protocol != NextProto::kProtoQUIC &&
        origin_quic_version_.IsKnown()) {
      StartAltSvcQuicPreconnect();
    }
    CHECK(!stream_ready_time_.has_value());
    stream_ready_time_ = base::TimeTicks::Now();
    TaskRunner(priority_)->PostTask(
        FROM_HERE,
        base::BindOnce(
            &HttpStreamPool::JobController::CallRequestCompleteAndStreamReady,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (!MaybeStartAlternativeJob()) {
    alternative_job_result_ = OK;
  }

  const bool alternative_job_succeeded = alternative_job_ &&
                                         alternative_job_result_.has_value() &&
                                         *alternative_job_result_ == OK;
  if (!alternative_job_succeeded) {
    origin_job_ =
        pool_->GetOrCreateGroup(origin_stream_key_, origin_quic_key_)
            .CreateJob(this, origin_quic_version_, NextProto::kProtoUnknown,
                       stream_request_->net_log());
    origin_job_->Start();
  }
}

int HttpStreamPool::JobController::Preconnect(
    size_t num_streams,
    CompletionOnceCallback callback) {
  TRACE_EVENT("net.stream", "JobController::Preconnect", flow_);

  num_streams = std::min(kDefaultMaxStreamSocketsPerGroup, num_streams);

  if (!IsPortAllowedForScheme(origin_stream_key_.destination().port(),
                              origin_stream_key_.destination().scheme())) {
    return ERR_UNSAFE_PORT;
  }

  if (CanUseExistingQuicSession()) {
    net_log_.AddEvent(
        NetLogEventType::
            HTTP_STREAM_POOL_JOB_CONTROLLER_FOUND_EXISTING_QUIC_SESSION);
    return OK;
  }

  // If the preconnect explicitly requests QUIC, start preconnecting before
  // checking existing SpdySession and idle streams.
  if (origin_quic_version_.IsKnown()) {
    preconnect_callback_ = std::move(callback);
    StartAltSvcQuicPreconnect();
    return ERR_IO_PENDING;
  }

  SpdySessionKey spdy_session_key =
      origin_stream_key_.CalculateSpdySessionKey();
  if (pool_->FindAvailableSpdySession(
          origin_stream_key_, spdy_session_key,
          /*enable_ip_based_pooling_for_h2=*/true)) {
    net_log_.AddEvent(
        NetLogEventType::
            HTTP_STREAM_POOL_JOB_CONTROLLER_FOUND_EXISTING_SPDY_SESSION);
    return OK;
  }

  Group& group = pool_->GetOrCreateGroup(origin_stream_key_, origin_quic_key_);
  if (group.ActiveStreamSocketCount() >= num_streams) {
    return OK;
  }

  if (pool_->delegate_for_testing_) {
    // Some tests expect OnPreconnect() is called after checking existing
    // sessions.
    std::optional<int> result = pool_->delegate_for_testing_->OnPreconnect(
        origin_stream_key_, num_streams);
    if (result.has_value()) {
      return *result;
    }
  }

  preconnect_callback_ = std::move(callback);
  origin_job_ = std::make_unique<Job>(
      this, JobType::kPreconnect, &group, origin_quic_version_,
      NextProto::kProtoUnknown, net_log_, num_streams);
  origin_job_->Start();
  return ERR_IO_PENDING;
}

RequestPriority HttpStreamPool::JobController::priority() const {
  return priority_;
}

HttpStreamPool::RespectLimits HttpStreamPool::JobController::respect_limits()
    const {
  return respect_limits_;
}

const std::vector<SSLConfig::CertAndStatus>&
HttpStreamPool::JobController::allowed_bad_certs() const {
  return allowed_bad_certs_;
}

bool HttpStreamPool::JobController::enable_ip_based_pooling_for_h2() const {
  return enable_ip_based_pooling_for_h2_;
}

bool HttpStreamPool::JobController::enable_alternative_services() const {
  return enable_alternative_services_;
}

NextProtoSet HttpStreamPool::JobController::allowed_alpns() const {
  return allowed_alpns_;
}

const ProxyInfo& HttpStreamPool::JobController::proxy_info() const {
  return proxy_info_;
}

const NetLogWithSource& HttpStreamPool::JobController::net_log() const {
  return net_log_;
}

const perfetto::Flow& HttpStreamPool::JobController::flow() const {
  return flow_;
}

void HttpStreamPool::JobController::OnStreamReady(
    Job* job,
    std::unique_ptr<HttpStream> stream,
    NextProto negotiated_protocol,
    std::optional<SessionSource> session_source) {
  TRACE_EVENT("net.stream", "JobController::OnStreamReady", flow_);

  SetJobResult(job, OK);

  // If there's already a `pending_stream_` or the callback has already been
  // invoked, nothing more to do.
  if (pending_stream_) {
    return;
  }

  pending_stream_.emplace(std::move(stream), negotiated_protocol,
                          session_source);
  CHECK(!stream_ready_time_.has_value());
  stream_ready_time_ = base::TimeTicks::Now();

  // Use PostTask to align the behavior with HttpStreamFactory::Job, see
  // https://crrev.com/2827533002.
  // TODO(crbug.com/346835898): Avoid using PostTask here if possible.
  TaskRunner(priority_)->PostTask(
      FROM_HERE,
      base::BindOnce(&JobController::CallRequestCompleteAndStreamReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HttpStreamPool::JobController::OnStreamFailed(
    Job* job,
    int status,
    const NetErrorDetails& net_error_details,
    ResolveErrorInfo resolve_error_info) {
  TRACE_EVENT("net.stream", "JobController::OnStreamFailed", flow_, "result",
              status);

  stream_request_->AddConnectionAttempts(job->connection_attempts());
  SetJobResult(job, status);
  if (AllJobsFinished()) {
    // Use PostTask to align the behavior with HttpStreamFactory::Job, see
    // https://crrev.com/2827533002.
    // TODO(crbug.com/346835898): Avoid using PostTask here if possible.
    TaskRunner(priority_)->PostTask(
        FROM_HERE,
        base::BindOnce(&JobController::CallOnStreamFailed,
                       weak_ptr_factory_.GetWeakPtr(), status,
                       net_error_details, std::move(resolve_error_info)));
  }
}

void HttpStreamPool::JobController::OnCertificateError(
    Job* job,
    int status,
    const SSLInfo& ssl_info) {
  TRACE_EVENT("net.stream", "JobController::OnCertificateError", flow_,
              "result", status);

  stream_request_->AddConnectionAttempts(job->connection_attempts());
  CancelOtherJob(job);
  // Use PostTask to align the behavior with HttpStreamFactory::Job, see
  // https://crrev.com/2827533002.
  // TODO(crbug.com/346835898): Avoid using PostTask here if possible.
  TaskRunner(priority_)->PostTask(
      FROM_HERE,
      base::BindOnce(&JobController::CallOnCertificateError,
                     weak_ptr_factory_.GetWeakPtr(), status, ssl_info));
}

void HttpStreamPool::JobController::OnNeedsClientAuth(
    Job* job,
    SSLCertRequestInfo* cert_info) {
  TRACE_EVENT("net.stream", "JobController::OnNeedsClientAuth", flow_);

  stream_request_->AddConnectionAttempts(job->connection_attempts());
  CancelOtherJob(job);
  // Use PostTask to align the behavior with HttpStreamFactory::Job, see
  // https://crrev.com/2827533002.
  // TODO(crbug.com/346835898): Avoid using PostTask here if possible.
  TaskRunner(priority_)->PostTask(
      FROM_HERE, base::BindOnce(&JobController::CallOnNeedsClientAuth,
                                weak_ptr_factory_.GetWeakPtr(),
                                base::RetainedRef(cert_info)));
}

void HttpStreamPool::JobController::OnPreconnectComplete(Job* job, int status) {
  TRACE_EVENT("net.stream", "JobController::OnPreconnectComplete", flow_,
              "result", status);
  TaskRunner(priority_)->PostTask(
      FROM_HERE,
      base::BindOnce(&JobController::ResetJobAndInvokePreconnectCallback,
                     weak_ptr_factory_.GetWeakPtr(), job, status));
}

LoadState HttpStreamPool::JobController::GetLoadState() const {
  CHECK(stream_request_);
  if (stream_request_->completed()) {
    return LOAD_STATE_IDLE;
  }

  if (origin_job_) {
    return origin_job_->GetLoadState();
  }
  if (alternative_job_) {
    return alternative_job_->GetLoadState();
  }
  return LOAD_STATE_IDLE;
}

void HttpStreamPool::JobController::OnRequestComplete() {
  delegate_ = nullptr;
  stream_request_ = nullptr;

  origin_job_.reset();
  alternative_job_.reset();
  MaybeMarkAlternativeServiceBroken();

  pool_->OnJobControllerComplete(this);
  // `this` is deleted.
}

int HttpStreamPool::JobController::RestartTunnelWithProxyAuth() {
  NOTREACHED();
}

void HttpStreamPool::JobController::SetPriority(RequestPriority priority) {
  priority_ = priority;
  if (origin_job_) {
    origin_job_->SetPriority(priority);
  }
  if (alternative_job_) {
    alternative_job_->SetPriority(priority);
  }
}

base::Value::Dict HttpStreamPool::JobController::GetInfoAsValue() const {
  base::Value::Dict dict;
  dict.Set("origin_stream_key", origin_stream_key_.ToValue());
  if (alternative_.has_value()) {
    dict.Set("alternative_stream_key", alternative_->stream_key.ToValue());
  }
  base::TimeDelta elapsed = base::TimeTicks::Now() - created_time_;
  dict.Set("elapsed_ms", static_cast<int>(elapsed.InMilliseconds()));
  return dict;
}

QuicSessionPool* HttpStreamPool::JobController::quic_session_pool() {
  return pool_->http_network_session()->quic_session_pool();
}

SpdySessionPool* HttpStreamPool::JobController::spdy_session_pool() {
  return pool_->http_network_session()->spdy_session_pool();
}

std::optional<HttpStreamPool::JobController::PendingStream>
HttpStreamPool::JobController::MaybeCreateStreamFromExistingSession() {
  // Check QUIC session first.
  std::unique_ptr<HttpStream> quic_http_stream =
      MaybeCreateStreamFromExistingQuicSession();
  if (quic_http_stream) {
    net_log_.AddEvent(
        NetLogEventType::
            HTTP_STREAM_POOL_JOB_CONTROLLER_FOUND_EXISTING_QUIC_SESSION);
    return std::optional<PendingStream>(
        std::in_place, std::move(quic_http_stream), NextProto::kProtoQUIC,
        SessionSource::kExisting);
  }

  // Check SPDY session next.
  SpdySessionKey spdy_session_key =
      origin_stream_key_.CalculateSpdySessionKey();
  base::WeakPtr<SpdySession> spdy_session = pool_->FindAvailableSpdySession(
      origin_stream_key_, spdy_session_key, enable_ip_based_pooling_for_h2_,
      stream_request_->net_log());
  if (spdy_session) {
    net_log_.AddEvent(
        NetLogEventType::
            HTTP_STREAM_POOL_JOB_CONTROLLER_FOUND_EXISTING_SPDY_SESSION);
    auto http_stream = std::make_unique<SpdyHttpStream>(
        spdy_session, stream_request_->net_log().source(),
        spdy_session_pool()->GetDnsAliasesForSessionKey(spdy_session_key));
    return std::optional<PendingStream>(std::in_place, std::move(http_stream),
                                        NextProto::kProtoHTTP2,
                                        SessionSource::kExisting);
  }

  // Check idle HTTP/1.1 stream.
  Group& origin_group =
      pool_->GetOrCreateGroup(origin_stream_key_, origin_quic_key_);
  std::unique_ptr<StreamSocket> idle_stream_socket =
      origin_group.GetIdleStreamSocket();
  if (idle_stream_socket) {
    StreamSocketHandle::SocketReuseType reuse_type =
        idle_stream_socket->WasEverUsed()
            ? StreamSocketHandle::SocketReuseType::kReusedIdle
            : StreamSocketHandle::SocketReuseType::kUnusedIdle;
    NextProto negotiated_protocol = idle_stream_socket->GetNegotiatedProtocol();
    std::unique_ptr<HttpStream> http_stream =
        origin_group.CreateTextBasedStream(std::move(idle_stream_socket),
                                           reuse_type,
                                           LoadTimingInfo::ConnectTiming());
    return std::optional<PendingStream>(std::in_place, std::move(http_stream),
                                        negotiated_protocol,
                                        /*session_source=*/std::nullopt);
  }

  return std::nullopt;
}

std::unique_ptr<HttpStream>
HttpStreamPool::JobController::MaybeCreateStreamFromExistingQuicSession() {
  std::unique_ptr<HttpStream> stream =
      MaybeCreateStreamFromExistingQuicSessionInternal(origin_quic_key_);
  if (stream) {
    return stream;
  }

  if (alternative_.has_value() && alternative_->quic_key.has_value()) {
    stream = MaybeCreateStreamFromExistingQuicSessionInternal(
        *alternative_->quic_key);
  }

  return stream;
}

std::unique_ptr<HttpStream>
HttpStreamPool::JobController::MaybeCreateStreamFromExistingQuicSessionInternal(
    const QuicSessionAliasKey& key) {
  if (!key.destination().IsValid() ||
      !pool_->CanUseQuic(key.destination(),
                         key.session_key().network_anonymization_key(),
                         enable_alternative_services_)) {
    return nullptr;
  }

  QuicChromiumClientSession* quic_session =
      quic_session_pool()->FindExistingSession(key.session_key(),
                                               key.destination());
  if (!quic_session) {
    return nullptr;
  }

  return std::make_unique<QuicHttpStream>(
      quic_session->CreateHandle(key.destination()),
      quic_session->GetDnsAliasesForSessionKey(key.session_key()));
}

bool HttpStreamPool::JobController::MaybeStartAlternativeJob() {
  if (!alternative_.has_value()) {
    return false;
  }

  Group& alternative_group =
      pool_->GetOrCreateGroup(alternative_->stream_key, alternative_->quic_key);

  // We never put streams that are negotiated to use HTTP/2 as idle streams.
  // Don't start alternative job if there is an idle stream. See
  // HttpNetworkTransactionTest.AlternativeServiceShouldNotPoolToHttp11 for a
  // scenario where we don't want to start alternative job.
  if (alternative_group.IdleStreamSocketCount() > 0) {
    return false;
  }

  alternative_job_ = alternative_group.CreateJob(
      this, alternative_->quic_version, alternative_->protocol,
      stream_request_->net_log());
  alternative_job_->Start();
  return true;
}

bool HttpStreamPool::JobController::CanUseExistingQuicSession() {
  return pool_->CanUseExistingQuicSession(origin_quic_key_,
                                          enable_alternative_services_);
}

void HttpStreamPool::JobController::StartAltSvcQuicPreconnect() {
  Group& group = pool_->GetOrCreateGroup(origin_stream_key_, origin_quic_key_);
  if (preconnect_callback_.is_null()) {
    preconnect_callback_ = pool_->GetAltSvcQuicPreconnectCallback();
  }
  origin_job_ = std::make_unique<Job>(this, JobType::kAltSvcQuicPreconnect,
                                      &group, origin_quic_version_,
                                      NextProto::kProtoQUIC, net_log_,
                                      /*num_streams=*/1);
  origin_job_->Start();
}

void HttpStreamPool::JobController::CallRequestCompleteAndStreamReady() {
  CHECK(stream_request_);
  CHECK(delegate_);
  CHECK(pending_stream_);
  CHECK(stream_ready_time_.has_value());

  base::TimeTicks now = base::TimeTicks::Now();
  base::UmaHistogramLongTimes100(
      base::StrCat({"Net.HttpStreamPool.JobControllerRequestCompleteTime2.",
                    NegotiatedProtocolToHistogramSuffixCoalesced(
                        pending_stream_->negotiated_protocol)}),
      now - created_time_);
  base::UmaHistogramTimes(
      "Net.HttpStreamPool.JobControllerCallRequestCompleteDelay",
      now - *stream_ready_time_);

  stream_request_->Complete({
      .negotiated_protocol = pending_stream_->negotiated_protocol,
      .alternate_protocol_usage = ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON,
      .session_source = pending_stream_->session_source,
      .advertised_alt_svc_state = advertised_alt_svc_state_,
  });
  delegate_->OnStreamReady(proxy_info_, std::move(pending_stream_->stream));
}

void HttpStreamPool::JobController::CallOnStreamFailed(
    int status,
    const NetErrorDetails& net_error_details,
    ResolveErrorInfo resolve_error_info) {
  delegate_->OnStreamFailed(status, net_error_details, proxy_info_,
                            std::move(resolve_error_info));
}

void HttpStreamPool::JobController::CallOnCertificateError(
    int status,
    const SSLInfo& ssl_info) {
  delegate_->OnCertificateError(status, ssl_info);
}

void HttpStreamPool::JobController::CallOnNeedsClientAuth(
    SSLCertRequestInfo* cert_info) {
  delegate_->OnNeedsClientAuth(cert_info);
}

void HttpStreamPool::JobController::ResetJobAndInvokePreconnectCallback(
    Job* job,
    int status) {
  CHECK(!alternative_job_);
  CHECK_EQ(origin_job_.get(), job);
  origin_job_.reset();
  if (preconnect_callback_) {
    std::move(preconnect_callback_).Run(status);
  }
}

void HttpStreamPool::JobController::SetJobResult(Job* job, int status) {
  if (origin_job_.get() == job) {
    origin_job_result_ = status;
  } else if (alternative_job_.get() == job) {
    alternative_job_result_ = status;
  } else {
    NOTREACHED();
  }
}

void HttpStreamPool::JobController::CancelOtherJob(Job* job) {
  if (origin_job_.get() == job) {
    alternative_job_.reset();
  } else if (alternative_job_.get() == job) {
    origin_job_.reset();
  } else {
    NOTREACHED();
  }
}

bool HttpStreamPool::JobController::AllJobsFinished() {
  return origin_job_result_.has_value() && alternative_job_result_.has_value();
}

void HttpStreamPool::JobController::MaybeMarkAlternativeServiceBroken() {
  // If alternative job succeeds or not completed, no brokenness to report.
  if (!alternative_job_result_.has_value() || *alternative_job_result_ == OK) {
    return;
  }

  // No brokenness to report if the origin job fails.
  if (origin_job_result_.has_value() && *origin_job_result_ != OK) {
    return;
  }

  CHECK(alternative_.has_value());

  pool_->http_network_session()
      ->http_server_properties()
      ->MarkAlternativeServiceBroken(
          alternative_service_info_.alternative_service(),
          alternative_->stream_key.network_anonymization_key());
}

}  // namespace net

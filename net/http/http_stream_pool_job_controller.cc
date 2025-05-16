// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_job_controller.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/load_flags.h"
#include "net/base/load_states.h"
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
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_http_stream.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {

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

  HttpStreamKey stream_key(
      destination, request_info.privacy_mode, request_info.socket_tag,
      request_info.network_anonymization_key, request_info.secure_dns_policy,
      request_info.disable_cert_network_fetches);

  Alternative alternative = {
      .stream_key = std::move(stream_key),
      .protocol = request_info.alternative_service_info.protocol(),
      .quic_version = quic::ParsedQuicVersion::Unsupported()};

  if (protocol == NextProto::kProtoQUIC) {
    alternative.quic_version =
        pool->SelectQuicVersion(request_info.alternative_service_info);
    alternative.quic_key =
        origin_stream_key.CalculateQuicSessionAliasKey(std::move(destination));
  }

  return alternative;
}

HttpStreamPool::JobController::JobController(
    HttpStreamPool* pool,
    HttpStreamPoolRequestInfo request_info,
    RequestPriority priority,
    std::vector<SSLConfig::CertAndStatus> allowed_bad_certs,
    bool enable_ip_based_pooling,
    bool enable_alternative_services)
    : pool_(pool),
      priority_(priority),
      allowed_bad_certs_(std::move(allowed_bad_certs)),
      enable_ip_based_pooling_(enable_ip_based_pooling),
      enable_alternative_services_(enable_alternative_services),
      respect_limits_(request_info.load_flags & LOAD_IGNORE_LIMITS
                          ? RespectLimits::kIgnore
                          : RespectLimits::kRespect),
      is_http1_allowed_(request_info.is_http1_allowed),
      proxy_info_(request_info.proxy_info),
      alternative_service_info_(request_info.alternative_service_info),
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
      created_time_(base::TimeTicks::Now()) {
  net_log_.BeginEvent(
      NetLogEventType::HTTP_STREAM_POOL_JOB_CONTROLLER_ALIVE, [&] {
        base::Value::Dict dict;
        dict.Set("origin_destination",
                 origin_stream_key_.destination().Serialize());
        if (alternative_.has_value()) {
          dict.Set("alternative_destination",
                   alternative_->stream_key.destination().Serialize());
        }
        dict.Set("enable_ip_based_pooling", enable_ip_based_pooling_);
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
}

std::unique_ptr<HttpStreamRequest> HttpStreamPool::JobController::RequestStream(
    HttpStreamRequest::Delegate* delegate,
    const NetLogWithSource& net_log) {
  CHECK(!delegate_);
  CHECK(!stream_request_);

  if (pool_->delegate_for_testing_) {
    pool_->delegate_for_testing_->OnRequestStream(origin_stream_key_);
  }

  delegate_ = delegate;
  auto stream_request = std::make_unique<HttpStreamRequest>(
      this, /*websocket_handshake_stream_create_helper=*/nullptr, net_log,
      HttpStreamRequest::HTTP_STREAM);
  stream_request_ = stream_request.get();

  if (!IsPortAllowedForScheme(origin_stream_key_.destination().port(),
                              origin_stream_key_.destination().scheme())) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&HttpStreamPool::JobController::CallOnStreamFailed,
                       weak_ptr_factory_.GetWeakPtr(), ERR_UNSAFE_PORT,
                       NetErrorDetails(), ResolveErrorInfo()));
    return stream_request;
  }

  std::unique_ptr<HttpStream> quic_http_stream =
      MaybeCreateStreamFromExistingQuicSession();
  if (quic_http_stream) {
    net_log_.AddEvent(
        NetLogEventType::
            HTTP_STREAM_POOL_JOB_CONTROLLER_FOUND_EXISTING_QUIC_SESSION);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &HttpStreamPool::JobController::CallRequestCompleteAndStreamReady,
            weak_ptr_factory_.GetWeakPtr(), std::move(quic_http_stream),
            NextProto::kProtoQUIC));
    return stream_request;
  }

  SpdySessionKey spdy_session_key =
      origin_stream_key_.CalculateSpdySessionKey();
  base::WeakPtr<SpdySession> spdy_session = pool_->FindAvailableSpdySession(
      origin_stream_key_, spdy_session_key, enable_ip_based_pooling_, net_log);
  if (spdy_session) {
    net_log_.AddEvent(
        NetLogEventType::
            HTTP_STREAM_POOL_JOB_CONTROLLER_FOUND_EXISTING_SPDY_SESSION);
    auto http_stream = std::make_unique<SpdyHttpStream>(
        spdy_session, net_log.source(),
        spdy_session_pool()->GetDnsAliasesForSessionKey(spdy_session_key));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &HttpStreamPool::JobController::CallRequestCompleteAndStreamReady,
            weak_ptr_factory_.GetWeakPtr(), std::move(http_stream),
            NextProto::kProtoHTTP2));
    return stream_request;
  }

  if (alternative_.has_value()) {
    alternative_job_ =
        pool_
            ->GetOrCreateGroup(alternative_->stream_key, alternative_->quic_key)
            .CreateJob(this, alternative_->quic_version, alternative_->protocol,
                       net_log);
    alternative_job_->Start();
  } else {
    alternative_job_result_ = OK;
  }

  const bool alternative_job_succeeded = alternative_job_ &&
                                         alternative_job_result_.has_value() &&
                                         *alternative_job_result_ == OK;
  if (!alternative_job_succeeded) {
    origin_job_ = pool_->GetOrCreateGroup(origin_stream_key_, origin_quic_key_)
                      .CreateJob(this, origin_quic_version_,
                                 NextProto::kProtoUnknown, net_log);
    origin_job_->Start();
  }

  return stream_request;
}

int HttpStreamPool::JobController::Preconnect(
    size_t num_streams,
    CompletionOnceCallback callback) {
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

  SpdySessionKey spdy_session_key =
      origin_stream_key_.CalculateSpdySessionKey();
  if (pool_->FindAvailableSpdySession(origin_stream_key_, spdy_session_key,
                                      /*enable_ip_based_pooling=*/true)) {
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
  origin_job_ =
      std::make_unique<Job>(this, &group, origin_quic_version_,
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

bool HttpStreamPool::JobController::enable_ip_based_pooling() const {
  return enable_ip_based_pooling_;
}

bool HttpStreamPool::JobController::enable_alternative_services() const {
  return enable_alternative_services_;
}

bool HttpStreamPool::JobController::is_http1_allowed() const {
  return is_http1_allowed_;
}

const ProxyInfo& HttpStreamPool::JobController::proxy_info() const {
  return proxy_info_;
}

const NetLogWithSource& HttpStreamPool::JobController::net_log() const {
  return net_log_;
}

void HttpStreamPool::JobController::OnStreamReady(
    Job* job,
    std::unique_ptr<HttpStream> stream,
    NextProto negotiated_protocol) {
  SetJobResult(job, OK);
  // Use PostTask to align the behavior with HttpStreamFactory::Job, see
  // https://crrev.com/2827533002.
  // TODO(crbug.com/346835898): Avoid using PostTask here if possible.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&JobController::CallRequestCompleteAndStreamReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(stream),
                     negotiated_protocol));
}

void HttpStreamPool::JobController::OnStreamFailed(
    Job* job,
    int status,
    const NetErrorDetails& net_error_details,
    ResolveErrorInfo resolve_error_info) {
  stream_request_->AddConnectionAttempts(job->connection_attempts());
  SetJobResult(job, status);
  if (AllJobsFinished()) {
    // Use PostTask to align the behavior with HttpStreamFactory::Job, see
    // https://crrev.com/2827533002.
    // TODO(crbug.com/346835898): Avoid using PostTask here if possible.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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
  stream_request_->AddConnectionAttempts(job->connection_attempts());
  CancelOtherJob(job);
  // Use PostTask to align the behavior with HttpStreamFactory::Job, see
  // https://crrev.com/2827533002.
  // TODO(crbug.com/346835898): Avoid using PostTask here if possible.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&JobController::CallOnCertificateError,
                     weak_ptr_factory_.GetWeakPtr(), status, ssl_info));
}

void HttpStreamPool::JobController::OnNeedsClientAuth(
    Job* job,
    SSLCertRequestInfo* cert_info) {
  stream_request_->AddConnectionAttempts(job->connection_attempts());
  CancelOtherJob(job);
  // Use PostTask to align the behavior with HttpStreamFactory::Job, see
  // https://crrev.com/2827533002.
  // TODO(crbug.com/346835898): Avoid using PostTask here if possible.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&JobController::CallOnNeedsClientAuth,
                                weak_ptr_factory_.GetWeakPtr(),
                                base::RetainedRef(cert_info)));
}

void HttpStreamPool::JobController::OnPreconnectComplete(Job* job, int status) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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

std::unique_ptr<HttpStream>
HttpStreamPool::JobController::MaybeCreateStreamFromExistingQuicSession() {
  std::unique_ptr<HttpStream> stream =
      MaybeCreateStreamFromExistingQuicSessionInternal(origin_quic_key_);
  if (stream) {
    return stream;
  }

  if (alternative_.has_value()) {
    stream = MaybeCreateStreamFromExistingQuicSessionInternal(
        alternative_->quic_key);
  }

  return stream;
}

std::unique_ptr<HttpStream>
HttpStreamPool::JobController::MaybeCreateStreamFromExistingQuicSessionInternal(
    const QuicSessionAliasKey& key) {
  if (!key.destination().IsValid() ||
      !pool_->CanUseQuic(
          key.destination(), key.session_key().network_anonymization_key(),
          enable_ip_based_pooling_, enable_alternative_services_)) {
    return nullptr;
  }

  QuicChromiumClientSession* quic_session =
      quic_session_pool()->FindExistingSession(key.session_key(),
                                               key.destination());
  if (quic_session) {
    return std::make_unique<QuicHttpStream>(
        quic_session->CreateHandle(key.destination()),
        quic_session->GetDnsAliasesForSessionKey(key.session_key()));
  }

  if (alternative_.has_value()) {
    return nullptr;
  }

  return nullptr;
}

bool HttpStreamPool::JobController::CanUseExistingQuicSession() {
  return pool_->CanUseExistingQuicSession(
      origin_quic_key_, enable_ip_based_pooling_, enable_alternative_services_);
}

void HttpStreamPool::JobController::CallRequestCompleteAndStreamReady(
    std::unique_ptr<HttpStream> stream,
    NextProto negotiated_protocol) {
  CHECK(stream_request_);
  CHECK(delegate_);
  stream_request_->Complete(negotiated_protocol,
                            ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON);
  delegate_->OnStreamReady(proxy_info_, std::move(stream));
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
  CHECK(preconnect_callback_);
  origin_job_.reset();
  std::move(preconnect_callback_).Run(status);
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

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_job_controller.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
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
#include "net/http/http_stream_pool_switching_info.h"
#include "net/http/http_stream_request.h"
#include "net/quic/quic_http_stream.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {

HttpStreamPool::JobController::JobController(HttpStreamPool* pool)
    : pool_(pool) {}

HttpStreamPool::JobController::~JobController() = default;

std::unique_ptr<HttpStreamRequest> HttpStreamPool::JobController::RequestStream(
    HttpStreamRequest::Delegate* delegate,
    HttpStreamPoolSwitchingInfo switching_info,
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    bool enable_ip_based_pooling,
    bool enable_alternative_services,
    const NetLogWithSource& net_log) {
  CHECK(switching_info.proxy_info.is_direct());
  CHECK(!delegate_);
  CHECK(!request_);

  if (pool_->delegate_for_testing_) {
    pool_->delegate_for_testing_->OnRequestStream(switching_info.stream_key);
  }

  delegate_ = delegate;
  auto request = std::make_unique<HttpStreamRequest>(
      this, /*websocket_handshake_stream_create_helper=*/nullptr, net_log,
      HttpStreamRequest::HTTP_STREAM);
  request_ = request.get();

  const HttpStreamKey& stream_key = switching_info.stream_key;

  network_anonymization_key_ = stream_key.network_anonymization_key();
  proxy_info_ = switching_info.proxy_info;

  QuicSessionAliasKey quic_session_alias_key =
      stream_key.CalculateQuicSessionAliasKey();
  if (pool_->CanUseExistingQuicSession(quic_session_alias_key,
                                       enable_ip_based_pooling,
                                       enable_alternative_services)) {
    QuicChromiumClientSession* quic_session =
        quic_session_pool()->FindExistingSession(
            quic_session_alias_key.session_key(),
            quic_session_alias_key.destination());
    auto http_stream = std::make_unique<QuicHttpStream>(
        quic_session->CreateHandle(stream_key.destination()),
        quic_session->GetDnsAliasesForSessionKey(
            quic_session_alias_key.session_key()));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&HttpStreamPool::JobController::CallRequestComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(http_stream),
                       NextProto::kProtoQUIC));
    return request;
  }

  SpdySessionKey spdy_session_key = stream_key.CalculateSpdySessionKey();
  base::WeakPtr<SpdySession> spdy_session = pool_->FindAvailableSpdySession(
      stream_key, spdy_session_key, enable_ip_based_pooling, net_log);
  if (spdy_session) {
    auto http_stream = std::make_unique<SpdyHttpStream>(
        spdy_session, net_log.source(),
        spdy_session_pool()->GetDnsAliasesForSessionKey(spdy_session_key));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&HttpStreamPool::JobController::CallRequestComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(http_stream),
                       NextProto::kProtoHTTP2));
    return request;
  }

  RespectLimits respect_limits =
      (switching_info.load_flags & LOAD_IGNORE_LIMITS)
          ? RespectLimits::kIgnore
          : RespectLimits::kRespect;

  // Currently we only support a single HTTP/2 alternative service. This
  // behavior is the same as HttpStreamFactory::JobController.
  if (switching_info.alternative_service_info.protocol() ==
      NextProto::kProtoHTTP2) {
    HttpStreamKey alt_stream_key(
        url::SchemeHostPort(
            url::kHttpsScheme,
            switching_info.alternative_service_info.host_port_pair().host(),
            switching_info.alternative_service_info.host_port_pair().port()),
        stream_key.privacy_mode(), stream_key.socket_tag(),
        stream_key.network_anonymization_key(), stream_key.secure_dns_policy(),
        stream_key.disable_cert_network_fetches());

    alternative_service_info_ = switching_info.alternative_service_info;

    alternative_job_ =
        pool_->GetOrCreateGroup(alt_stream_key)
            .CreateJob(this, alternative_service_info_.protocol(),
                       switching_info.is_http1_allowed,
                       switching_info.proxy_info);
    alternative_job_->Start(priority, allowed_bad_certs, respect_limits,
                            enable_ip_based_pooling,
                            enable_alternative_services,
                            quic::ParsedQuicVersion::Unsupported(), net_log);
  } else {
    alternative_job_result_ = OK;
  }

  quic::ParsedQuicVersion quic_version =
      pool_->SelectQuicVersion(switching_info.alternative_service_info);
  origin_job_ =
      pool_->GetOrCreateGroup(stream_key, std::move(quic_session_alias_key))
          .CreateJob(this, NextProto::kProtoUnknown,
                     switching_info.is_http1_allowed,
                     switching_info.proxy_info);
  origin_job_->Start(priority, allowed_bad_certs, respect_limits,
                     enable_ip_based_pooling, enable_alternative_services,
                     quic_version, net_log);

  return request;
}

int HttpStreamPool::JobController::Preconnect(
    HttpStreamPoolSwitchingInfo switching_info,
    size_t num_streams,
    CompletionOnceCallback callback) {
  num_streams = std::min(kDefaultMaxStreamSocketsPerGroup, num_streams);

  const HttpStreamKey& stream_key = switching_info.stream_key;
  if (!IsPortAllowedForScheme(stream_key.destination().port(),
                              stream_key.destination().scheme())) {
    return ERR_UNSAFE_PORT;
  }

  QuicSessionAliasKey quic_session_alias_key =
      stream_key.CalculateQuicSessionAliasKey();
  if (pool_->CanUseExistingQuicSession(quic_session_alias_key,
                                       /*enable_ip_based_pooling=*/true,
                                       /*enable_alternative_services=*/true)) {
    return OK;
  }

  SpdySessionKey spdy_session_key = stream_key.CalculateSpdySessionKey();
  bool had_spdy_session = spdy_session_pool()->HasAvailableSession(
      spdy_session_key, /*is_websocket=*/false);
  if (pool_->FindAvailableSpdySession(stream_key, spdy_session_key,
                                      /*enable_ip_based_pooling=*/true)) {
    return OK;
  }
  if (had_spdy_session) {
    // We had a SPDY session but the server required HTTP/1.1. The session is
    // going away right now.
    return ERR_HTTP_1_1_REQUIRED;
  }

  if (pool_->delegate_for_testing_) {
    // Some tests expect OnPreconnect() is called after checking existing
    // sessions.
    std::optional<int> result =
        pool_->delegate_for_testing_->OnPreconnect(stream_key, num_streams);
    if (result.has_value()) {
      return *result;
    }
  }

  quic::ParsedQuicVersion quic_version =
      pool_->SelectQuicVersion(switching_info.alternative_service_info);
  return pool_->GetOrCreateGroup(stream_key, std::move(quic_session_alias_key))
      .Preconnect(num_streams, quic_version, std::move(callback));
}

void HttpStreamPool::JobController::OnStreamReady(
    Job* job,
    std::unique_ptr<HttpStream> stream,
    NextProto negotiated_protocol) {
  SetJobResult(job, OK);
  request_->Complete(negotiated_protocol,
                     ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON);
  // `job` should not be destroyed yet.
  delegate_->OnStreamReady(job->proxy_info(), std::move(stream));
}

void HttpStreamPool::JobController::OnStreamFailed(
    Job* job,
    int status,
    const NetErrorDetails& net_error_details,
    ResolveErrorInfo resolve_error_info) {
  request_->AddConnectionAttempts(job->connection_attempts());
  SetJobResult(job, status);
  if (AllJobsFinished()) {
    // `job` should not be destroyed yet.
    delegate_->OnStreamFailed(status, net_error_details, job->proxy_info(),
                              std::move(resolve_error_info));
  }
}

void HttpStreamPool::JobController::OnCertificateError(
    Job* job,
    int status,
    const SSLInfo& ssl_info) {
  request_->AddConnectionAttempts(job->connection_attempts());
  CancelOtherJob(job);
  delegate_->OnCertificateError(status, ssl_info);
}

void HttpStreamPool::JobController::OnNeedsClientAuth(
    Job* job,
    SSLCertRequestInfo* cert_info) {
  request_->AddConnectionAttempts(job->connection_attempts());
  CancelOtherJob(job);
  delegate_->OnNeedsClientAuth(cert_info);
}

LoadState HttpStreamPool::JobController::GetLoadState() const {
  CHECK(request_);
  if (request_->completed()) {
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
  request_ = nullptr;

  origin_job_.reset();
  alternative_job_.reset();
  MaybeMarkAlternativeServiceBroken();

  pool_->OnJobControllerComplete(this);
}

int HttpStreamPool::JobController::RestartTunnelWithProxyAuth() {
  NOTREACHED();
}

void HttpStreamPool::JobController::SetPriority(RequestPriority priority) {
  if (origin_job_) {
    origin_job_->SetPriority(priority);
  }
  if (alternative_job_) {
    alternative_job_->SetPriority(priority);
  }
}

QuicSessionPool* HttpStreamPool::JobController::quic_session_pool() {
  return pool_->http_network_session()->quic_session_pool();
}

SpdySessionPool* HttpStreamPool::JobController::spdy_session_pool() {
  return pool_->http_network_session()->spdy_session_pool();
}

void HttpStreamPool::JobController::CallRequestComplete(
    std::unique_ptr<HttpStream> stream,
    NextProto negotiated_protocol) {
  CHECK(request_);
  CHECK(delegate_);
  request_->Complete(negotiated_protocol,
                     ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON);
  delegate_->OnStreamReady(proxy_info_, std::move(stream));
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

  pool_->http_network_session()
      ->http_server_properties()
      ->MarkAlternativeServiceBroken(
          alternative_service_info_.alternative_service(),
          network_anonymization_key_);
}

}  // namespace net

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_job_controller.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "net/base/load_flags.h"
#include "net/base/load_states.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
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
#include "net/socket/next_proto.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "url/scheme_host_port.h"

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
  CHECK(!delegate_);
  CHECK(!request_);

  const HttpStreamKey& stream_key = switching_info.stream_key;

  network_anonymization_key_ = stream_key.network_anonymization_key();

  delegate_ = delegate;
  auto request = std::make_unique<HttpStreamRequest>(
      this, /*websocket_handshake_stream_create_helper=*/nullptr, net_log,
      HttpStreamRequest::HTTP_STREAM);
  request_ = request.get();

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

    alternative_service_info_ =
        std::move(switching_info.alternative_service_info);

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

  origin_job_ = pool_->GetOrCreateGroup(stream_key)
                    .CreateJob(this, NextProto::kProtoUnknown,
                               switching_info.is_http1_allowed,
                               switching_info.proxy_info);
  origin_job_->Start(priority, allowed_bad_certs, respect_limits,
                     enable_ip_based_pooling, enable_alternative_services,
                     switching_info.quic_version, net_log);

  return request;
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
  NOTREACHED_NORETURN();
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
  NOTREACHED_NORETURN();
}

void HttpStreamPool::JobController::SetPriority(RequestPriority priority) {
  if (origin_job_) {
    origin_job_->SetPriority(priority);
  }
  if (alternative_job_) {
    alternative_job_->SetPriority(priority);
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

  pool_->http_network_session()
      ->http_server_properties()
      ->MarkAlternativeServiceBroken(
          alternative_service_info_.alternative_service(),
          network_anonymization_key_);
}

}  // namespace net

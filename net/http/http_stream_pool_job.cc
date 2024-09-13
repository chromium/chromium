// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_job.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/port_util.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_attempt_manager.h"
#include "net/http/http_stream_pool_group.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

HttpStreamPool::Job::Job(Delegate* delegate,
                         AttemptManager* attempt_manager,
                         NextProto expected_protocol,
                         bool is_http1_allowed,
                         ProxyInfo proxy_info)
    : delegate_(delegate),
      attempt_manager_(attempt_manager),
      expected_protocol_(expected_protocol),
      is_http1_allowed_(is_http1_allowed),
      proxy_info_(std::move(proxy_info)) {
  CHECK(is_http1_allowed_ || expected_protocol_ != NextProto::kProtoHTTP11);
}

HttpStreamPool::Job::~Job() {
  CHECK(attempt_manager_);
  // `attempt_manager_` may be deleted after this call.
  attempt_manager_.ExtractAsDangling()->OnJobComplete(this);
}

void HttpStreamPool::Job::Start(
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    RespectLimits respect_limits,
    bool enable_ip_based_pooling,
    bool enable_alternative_services,
    quic::ParsedQuicVersion quic_version,
    const NetLogWithSource& net_log) {
  const url::SchemeHostPort& destination =
      attempt_manager_->group()->stream_key().destination();
  if (!IsPortAllowedForScheme(destination.port(), destination.scheme())) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&Job::OnStreamFailed, weak_ptr_factory_.GetWeakPtr(),
                       ERR_UNSAFE_PORT, NetErrorDetails(), ResolveErrorInfo()));
    return;
  }

  attempt_manager_->group()->StartJob(this, priority, allowed_bad_certs,
                                      respect_limits, enable_ip_based_pooling,
                                      enable_alternative_services, quic_version,
                                      net_log);
}

LoadState HttpStreamPool::Job::GetLoadState() const {
  CHECK(attempt_manager_);
  return attempt_manager_->GetLoadState();
}

void HttpStreamPool::Job::SetPriority(RequestPriority priority) {
  CHECK(attempt_manager_);
  attempt_manager_->SetJobPriority(this, priority);
}

void HttpStreamPool::Job::AddConnectionAttempts(
    const ConnectionAttempts& attempts) {
  for (const auto& attempt : attempts) {
    connection_attempts_.emplace_back(attempt);
  }
}

void HttpStreamPool::Job::OnStreamReady(std::unique_ptr<HttpStream> stream,
                                        NextProto negotiated_protocol) {
  int result = OK;
  if (expected_protocol_ != NextProto::kProtoUnknown &&
      expected_protocol_ != negotiated_protocol) {
    result = ERR_ALPN_NEGOTIATION_FAILED;
  } else if (!is_http1_allowed_ &&
             !(negotiated_protocol == NextProto::kProtoHTTP2 ||
               negotiated_protocol == NextProto::kProtoQUIC)) {
    result = ERR_H2_OR_QUIC_REQUIRED;
  }

  if (result != OK) {
    OnStreamFailed(result, NetErrorDetails(), ResolveErrorInfo());
    return;
  }

  attempt_manager_->group()
      ->http_network_session()
      ->proxy_resolution_service()
      ->ReportSuccess(proxy_info_);

  // Always use PostTask to align the behavior with HttpStreamFactory::Job, see
  // https://crrev.com/2827533002.
  // TODO(crbug.com/346835898): Avoid using PostTask here if possible.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&Job::CallOnStreamReady, weak_ptr_factory_.GetWeakPtr(),
                     std::move(stream), negotiated_protocol));
}

void HttpStreamPool::Job::OnStreamFailed(
    int status,
    const NetErrorDetails& net_error_details,
    ResolveErrorInfo resolve_error_info) {
  // Always use PostTask to align the behavior with HttpStreamFactory::Job, see
  // https://crrev.com/2827533002.
  // TODO(crbug.com/346835898): Avoid using PostTask here if possible.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&Job::CallOnStreamFailed, weak_ptr_factory_.GetWeakPtr(),
                     status, net_error_details, resolve_error_info));
}

void HttpStreamPool::Job::OnCertificateError(int status,
                                             const SSLInfo& ssl_info) {
  // Always use PostTask to align the behavior with HttpStreamFactory::Job, see
  // https://crrev.com/2827533002.
  // TODO(crbug.com/346835898): Avoid using PostTask here if possible.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&Job::CallOnCertificateError,
                     weak_ptr_factory_.GetWeakPtr(), status, ssl_info));
}

void HttpStreamPool::Job::OnNeedsClientAuth(SSLCertRequestInfo* cert_info) {
  // Always use PostTask to align the behavior with HttpStreamFactory::Job, see
  // https://crrev.com/2827533002.
  // TODO(crbug.com/346835898): Avoid using PostTask here if possible.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&Job::CallOnNeedsClientAuth,
                                weak_ptr_factory_.GetWeakPtr(),
                                base::RetainedRef(cert_info)));
}

void HttpStreamPool::Job::CallOnStreamReady(std::unique_ptr<HttpStream> stream,
                                            NextProto negotiated_protocol) {
  CHECK(delegate_);
  delegate_->OnStreamReady(this, std::move(stream), negotiated_protocol);
}

void HttpStreamPool::Job::CallOnStreamFailed(
    int status,
    const NetErrorDetails& net_error_details,
    ResolveErrorInfo resolve_error_info) {
  CHECK(delegate_);
  delegate_->OnStreamFailed(this, status, net_error_details,
                            resolve_error_info);
}

void HttpStreamPool::Job::CallOnCertificateError(int status,
                                                 const SSLInfo& ssl_info) {
  CHECK(delegate_);
  delegate_->OnCertificateError(this, status, ssl_info);
}

void HttpStreamPool::Job::CallOnNeedsClientAuth(SSLCertRequestInfo* cert_info) {
  CHECK(delegate_);
  delegate_->OnNeedsClientAuth(this, cert_info);
}

}  // namespace net

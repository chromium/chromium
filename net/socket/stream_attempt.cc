// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/stream_attempt.h"

#include <memory>

#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace net {

// static
StreamAttemptParams StreamAttemptParams::FromHttpNetworkSession(
    HttpNetworkSession* session) {
  return StreamAttemptParams(
      session->context().client_socket_factory, session->ssl_client_context(),
      session->context().socket_performance_watcher_factory,
      session->context().network_quality_estimator, session->net_log());
}

StreamAttemptParams::StreamAttemptParams(
    ClientSocketFactory* client_socket_factory,
    SSLClientContext* ssl_client_context,
    SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
    NetworkQualityEstimator* network_quality_estimator,
    NetLog* net_log)
    : client_socket_factory(client_socket_factory),
      ssl_client_context(ssl_client_context),
      socket_performance_watcher_factory(socket_performance_watcher_factory),
      network_quality_estimator(network_quality_estimator),
      net_log(net_log) {}

StreamAttempt::StreamAttempt(const StreamAttemptParams* params,
                             IPEndPoint ip_endpoint,
                             NetLogSourceType net_log_source_type,
                             NetLogEventType net_log_attempt_event_type,
                             const NetLogWithSource* net_log)
    : params_(params),
      ip_endpoint_(ip_endpoint),
      net_log_(net_log ? *net_log
                       : NetLogWithSource::Make(params->net_log,
                                                net_log_source_type)),
      net_log_attempt_event_type_(net_log_attempt_event_type) {}

StreamAttempt::~StreamAttempt() {
  // Log this attempt as aborted if the attempt was still in-progress when
  // destroyed.
  if (callback_) {
    LogCompletion(ERR_ABORTED);
  }
}

int StreamAttempt::Start(CompletionOnceCallback callback) {
  net_log().BeginEvent(net_log_attempt_event_type_,
                       [&] { return GetNetLogStartParams(); });

  int rv = StartInternal();
  if (rv != ERR_IO_PENDING) {
    LogCompletion(rv);
  } else {
    callback_ = std::move(callback);
  }
  return rv;
}

std::unique_ptr<StreamSocket> StreamAttempt::ReleaseStreamSocket() {
  return std::move(stream_socket_);
}

scoped_refptr<SSLCertRequestInfo> StreamAttempt::GetCertRequestInfo() {
  return nullptr;
}

void StreamAttempt::SetStreamSocket(std::unique_ptr<StreamSocket> socket) {
  stream_socket_ = std::move(socket);
}

void StreamAttempt::NotifyOfCompletion(int rv) {
  CHECK(callback_);

  LogCompletion(rv);
  std::move(callback_).Run(rv);
  // `this` may be deleted.
}

void StreamAttempt::LogCompletion(int rv) {
  connect_timing_.connect_end = base::TimeTicks::Now();
  net_log().EndEventWithNetErrorCode(net_log_attempt_event_type_, rv);
}

}  // namespace net

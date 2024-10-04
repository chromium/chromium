// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tls_stream_attempt.h"

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/tcp_stream_attempt.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace net {

TlsStreamAttempt::TlsStreamAttempt(const StreamAttemptParams* params,
                                   IPEndPoint ip_endpoint,
                                   HostPortPair host_port_pair,
                                   SSLConfigProvider* ssl_config_provider)
    : StreamAttempt(params,
                    ip_endpoint,
                    NetLogSourceType::TLS_STREAM_ATTEMPT,
                    NetLogEventType::TLS_STREAM_ATTEMPT_ALIVE),
      host_port_pair_(std::move(host_port_pair)),
      ssl_config_provider_(ssl_config_provider) {}

TlsStreamAttempt::~TlsStreamAttempt() = default;

LoadState TlsStreamAttempt::GetLoadState() const {
  switch (next_state_) {
    case State::kNone:
      return LOAD_STATE_IDLE;
    case State::kTcpAttempt:
    case State::kTcpAttemptComplete:
      CHECK(nested_attempt_);
      return nested_attempt_->GetLoadState();
    case State::kTlsAttempt:
    case State::kTlsAttemptComplete:
      return LOAD_STATE_SSL_HANDSHAKE;
  }
}

scoped_refptr<SSLCertRequestInfo> TlsStreamAttempt::GetCertRequestInfo() {
  return ssl_cert_request_info_;
}

void TlsStreamAttempt::SetTcpHandshakeCompletionCallback(
    CompletionOnceCallback callback) {
  CHECK(!tls_handshake_started_);
  CHECK(!tcp_handshake_completion_callback_);
  if (next_state_ <= State::kTcpAttemptComplete) {
    tcp_handshake_completion_callback_ = std::move(callback);
  }
}

int TlsStreamAttempt::StartInternal() {
  CHECK_EQ(next_state_, State::kNone);
  next_state_ = State::kTcpAttempt;
  return DoLoop(OK);
}

base::Value::Dict TlsStreamAttempt::GetNetLogStartParams() {
  base::Value::Dict dict;
  dict.Set("host_port", host_port_pair_.ToString());
  return dict;
}

void TlsStreamAttempt::OnIOComplete(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  rv = DoLoop(rv);
  if (rv != ERR_IO_PENDING) {
    NotifyOfCompletion(rv);
  }
}

int TlsStreamAttempt::DoLoop(int rv) {
  CHECK_NE(next_state_, State::kNone);

  do {
    State state = next_state_;
    next_state_ = State::kNone;
    switch (state) {
      case State::kNone:
        NOTREACHED() << "Invalid state";
      case State::kTcpAttempt:
        rv = DoTcpAttempt();
        break;
      case State::kTcpAttemptComplete:
        rv = DoTcpAttemptComplete(rv);
        break;
      case State::kTlsAttempt:
        rv = DoTlsAttempt(rv);
        break;
      case State::kTlsAttemptComplete:
        rv = DoTlsAttemptComplete(rv);
        break;
    }
  } while (next_state_ != State::kNone && rv != ERR_IO_PENDING);

  return rv;
}

int TlsStreamAttempt::DoTcpAttempt() {
  next_state_ = State::kTcpAttemptComplete;
  nested_attempt_ =
      std::make_unique<TcpStreamAttempt>(&params(), ip_endpoint(), &net_log());
  return nested_attempt_->Start(
      base::BindOnce(&TlsStreamAttempt::OnIOComplete, base::Unretained(this)));
}

int TlsStreamAttempt::DoTcpAttemptComplete(int rv) {
  const LoadTimingInfo::ConnectTiming& nested_timing =
      nested_attempt_->connect_timing();
  mutable_connect_timing().connect_start = nested_timing.connect_start;

  tcp_handshake_completed_ = true;
  if (tcp_handshake_completion_callback_) {
    std::move(tcp_handshake_completion_callback_).Run(rv);
  }

  if (rv != OK) {
    return rv;
  }

  net_log().BeginEvent(NetLogEventType::TLS_STREAM_ATTEMPT_WAIT_FOR_SSL_CONFIG);

  next_state_ = State::kTlsAttempt;
  return ssl_config_provider_->WaitForSSLConfigReady(
      base::BindOnce(&TlsStreamAttempt::OnIOComplete, base::Unretained(this)));
}

int TlsStreamAttempt::DoTlsAttempt(int rv) {
  CHECK_EQ(rv, OK);
  CHECK(ssl_config_provider_);

  net_log().EndEvent(NetLogEventType::TLS_STREAM_ATTEMPT_WAIT_FOR_SSL_CONFIG);

  next_state_ = State::kTlsAttemptComplete;

  std::unique_ptr<StreamSocket> nested_socket =
      nested_attempt_->ReleaseStreamSocket();
  SSLConfig ssl_config = ssl_config_provider_->GetSSLConfig();
  // Clear `ssl_config_provider_` to avoid dangling pointer.
  ssl_config_provider_ = nullptr;

  nested_attempt_.reset();

  tls_handshake_started_ = true;
  mutable_connect_timing().ssl_start = base::TimeTicks::Now();
  tls_handshake_timeout_timer_.Start(
      FROM_HERE, kTlsHandshakeTimeout,
      base::BindOnce(&TlsStreamAttempt::OnTlsHandshakeTimeout,
                     base::Unretained(this)));

  ssl_socket_ = params().client_socket_factory->CreateSSLClientSocket(
      params().ssl_client_context, std::move(nested_socket), host_port_pair_,
      ssl_config);

  net_log().BeginEvent(NetLogEventType::TLS_STREAM_ATTEMPT_CONNECT);

  return ssl_socket_->Connect(
      base::BindOnce(&TlsStreamAttempt::OnIOComplete, base::Unretained(this)));
}

int TlsStreamAttempt::DoTlsAttemptComplete(int rv) {
  CHECK(ssl_socket_);

  net_log().EndEventWithNetErrorCode(
      NetLogEventType::TLS_STREAM_ATTEMPT_CONNECT, rv);

  mutable_connect_timing().ssl_end = base::TimeTicks::Now();
  tls_handshake_timeout_timer_.Stop();

  // TODO(crbug.com/346835898): Support ECH.
  CHECK_NE(rv, ERR_ECH_NOT_NEGOTIATED) << "Not implemented yet";

  const bool is_ech_capable = false;
  const bool ech_enabled = params().ssl_client_context->config().ech_enabled;
  SSLClientSocket::RecordSSLConnectResult(
      *ssl_socket_, rv, is_ech_capable, ech_enabled,
      /*ech_retry_configs=*/std::nullopt, connect_timing());

  if (rv == OK || IsCertificateError(rv)) {
    SetStreamSocket(std::move(ssl_socket_));
  } else if (rv == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    ssl_cert_request_info_ = base::MakeRefCounted<SSLCertRequestInfo>();
    ssl_socket_->GetSSLCertRequestInfo(ssl_cert_request_info_.get());
  }

  return rv;
}

void TlsStreamAttempt::OnTlsHandshakeTimeout() {
  // TODO(bashi): The error code should be ERR_CONNECTION_TIMED_OUT but use
  // ERR_TIMED_OUT for consistency with ConnectJobs.
  OnIOComplete(ERR_TIMED_OUT);
}

}  // namespace net

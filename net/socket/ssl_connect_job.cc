// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_connect_job.h"

#include <cstdlib>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/base/url_util.h"
#include "net/cert/x509_util.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/transport_connect_job.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

// Timeout for the SSL handshake portion of the connect.
constexpr base::TimeDelta kSSLHandshakeTimeout(base::Seconds(30));

}  // namespace

SSLSocketParams::SSLSocketParams(
    ConnectJobParams nested_params,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    NetworkAnonymizationKey network_anonymization_key)
    : nested_params_(nested_params),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      network_anonymization_key_(network_anonymization_key) {
  CHECK(!nested_params_.is_ssl());
}

SSLSocketParams::~SSLSocketParams() = default;

SSLSocketParams::ConnectionType SSLSocketParams::GetConnectionType() const {
  if (nested_params_.is_socks()) {
    return SOCKS_PROXY;
  }
  if (nested_params_.is_http_proxy()) {
    return HTTP_PROXY;
  }
  return DIRECT;
}

std::unique_ptr<SSLConnectJob> SSLConnectJob::Factory::Create(
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    scoped_refptr<SSLSocketParams> params,
    ConnectJob::Delegate* delegate,
    const NetLogWithSource* net_log) {
  return std::make_unique<SSLConnectJob>(priority, socket_tag,
                                         common_connect_job_params,
                                         std::move(params), delegate, net_log);
}

SSLConnectJob::SSLConnectJob(
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    scoped_refptr<SSLSocketParams> params,
    ConnectJob::Delegate* delegate,
    const NetLogWithSource* net_log)
    : ConnectJob(
          priority,
          socket_tag,
          // The SSLConnectJob's timer is only started during the SSL handshake.
          base::TimeDelta(),
          common_connect_job_params,
          delegate,
          net_log,
          NetLogSourceType::SSL_CONNECT_JOB,
          NetLogEventType::SSL_CONNECT_JOB_CONNECT),
      params_(std::move(params)),
      callback_(base::BindRepeating(&SSLConnectJob::OnIOComplete,
                                    base::Unretained(this))) {}

SSLConnectJob::~SSLConnectJob() {
  // In the case the job was canceled, need to delete nested job first to
  // correctly order NetLog events.
  nested_connect_job_.reset();
}

LoadState SSLConnectJob::GetLoadState() const {
  switch (next_state_) {
    case STATE_TRANSPORT_CONNECT:
    case STATE_SOCKS_CONNECT:
    case STATE_TUNNEL_CONNECT:
      return LOAD_STATE_IDLE;
    case STATE_TRANSPORT_CONNECT_COMPLETE:
    case STATE_SOCKS_CONNECT_COMPLETE:
      return nested_connect_job_->GetLoadState();
    case STATE_TUNNEL_CONNECT_COMPLETE:
      if (nested_socket_) {
        return LOAD_STATE_ESTABLISHING_PROXY_TUNNEL;
      }
      return nested_connect_job_->GetLoadState();
    case STATE_SSL_CONNECT:
    case STATE_SSL_CONNECT_COMPLETE:
      return LOAD_STATE_SSL_HANDSHAKE;
    default:
      NOTREACHED_IN_MIGRATION();
      return LOAD_STATE_IDLE;
  }
}

bool SSLConnectJob::HasEstablishedConnection() const {
  // If waiting on a nested ConnectJob, defer to that ConnectJob's state.
  if (nested_connect_job_) {
    return nested_connect_job_->HasEstablishedConnection();
  }
  // Otherwise, return true if a socket has been created.
  return nested_socket_ || ssl_socket_;
}

void SSLConnectJob::OnConnectJobComplete(int result, ConnectJob* job) {
  DCHECK_EQ(job, nested_connect_job_.get());
  OnIOComplete(result);
}

void SSLConnectJob::OnNeedsProxyAuth(
    const HttpResponseInfo& response,
    HttpAuthController* auth_controller,
    base::OnceClosure restart_with_auth_callback,
    ConnectJob* job) {
  DCHECK_EQ(next_state_, STATE_TUNNEL_CONNECT_COMPLETE);

  // The timer shouldn't have started running yet, since the handshake only
  // starts after a tunnel has been established through the proxy.
  DCHECK(!TimerIsRunning());

  // Just pass the callback up to the consumer. This class doesn't need to do
  // anything once credentials are provided.
  NotifyDelegateOfProxyAuth(response, auth_controller,
                            std::move(restart_with_auth_callback));
}

ConnectionAttempts SSLConnectJob::GetConnectionAttempts() const {
  return connection_attempts_;
}

ResolveErrorInfo SSLConnectJob::GetResolveErrorInfo() const {
  return resolve_error_info_;
}

bool SSLConnectJob::IsSSLError() const {
  return ssl_negotiation_started_;
}

scoped_refptr<SSLCertRequestInfo> SSLConnectJob::GetCertRequestInfo() {
  return ssl_cert_request_info_;
}

base::TimeDelta SSLConnectJob::HandshakeTimeoutForTesting() {
  return kSSLHandshakeTimeout;
}

void SSLConnectJob::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    NotifyDelegateOfCompletion(rv);  // Deletes |this|.
  }
}

int SSLConnectJob::DoLoop(int result) {
  TRACE_EVENT0(NetTracingCategory(), "SSLConnectJob::DoLoop");
  DCHECK_NE(next_state_, STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_TRANSPORT_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoTransportConnect();
        break;
      case STATE_TRANSPORT_CONNECT_COMPLETE:
        rv = DoTransportConnectComplete(rv);
        break;
      case STATE_SOCKS_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoSOCKSConnect();
        break;
      case STATE_SOCKS_CONNECT_COMPLETE:
        rv = DoSOCKSConnectComplete(rv);
        break;
      case STATE_TUNNEL_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoTunnelConnect();
        break;
      case STATE_TUNNEL_CONNECT_COMPLETE:
        rv = DoTunnelConnectComplete(rv);
        break;
      case STATE_SSL_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoSSLConnect();
        break;
      case STATE_SSL_CONNECT_COMPLETE:
        rv = DoSSLConnectComplete(rv);
        break;
      default:
        NOTREACHED_IN_MIGRATION() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int SSLConnectJob::DoTransportConnect() {
  DCHECK(!nested_connect_job_);
  DCHECK(params_->GetDirectConnectionParams());
  DCHECK(!TimerIsRunning());

  next_state_ = STATE_TRANSPORT_CONNECT_COMPLETE;
  // If this is an ECH retry, connect to the same server as before.
  std::optional<TransportConnectJob::EndpointResultOverride>
      endpoint_result_override;
  if (ech_retry_configs_) {
    DCHECK(ssl_client_context()->config().ech_enabled);
    DCHECK(endpoint_result_);
    endpoint_result_override.emplace(*endpoint_result_, dns_aliases_);
  }
  nested_connect_job_ = std::make_unique<TransportConnectJob>(
      priority(), socket_tag(), common_connect_job_params(),
      params_->GetDirectConnectionParams(), this, &net_log(),
      std::move(endpoint_result_override));
  return nested_connect_job_->Connect();
}

int SSLConnectJob::DoTransportConnectComplete(int result) {
  resolve_error_info_ = nested_connect_job_->GetResolveErrorInfo();
  ConnectionAttempts connection_attempts =
      nested_connect_job_->GetConnectionAttempts();
  connection_attempts_.insert(connection_attempts_.end(),
                              connection_attempts.begin(),
                              connection_attempts.end());
  if (result == OK) {
    next_state_ = STATE_SSL_CONNECT;
    nested_socket_ = nested_connect_job_->PassSocket();
    nested_socket_->GetPeerAddress(&server_address_);
    dns_aliases_ = nested_socket_->GetDnsAliases();
  }

  return result;
}

int SSLConnectJob::DoSOCKSConnect() {
  DCHECK(!nested_connect_job_);
  DCHECK(params_->GetSocksProxyConnectionParams());
  DCHECK(!TimerIsRunning());

  next_state_ = STATE_SOCKS_CONNECT_COMPLETE;
  nested_connect_job_ = std::make_unique<SOCKSConnectJob>(
      priority(), socket_tag(), common_connect_job_params(),
      params_->GetSocksProxyConnectionParams(), this, &net_log());
  return nested_connect_job_->Connect();
}

int SSLConnectJob::DoSOCKSConnectComplete(int result) {
  resolve_error_info_ = nested_connect_job_->GetResolveErrorInfo();
  if (result == OK) {
    next_state_ = STATE_SSL_CONNECT;
    nested_socket_ = nested_connect_job_->PassSocket();
  }

  return result;
}

int SSLConnectJob::DoTunnelConnect() {
  DCHECK(!nested_connect_job_);
  DCHECK(params_->GetHttpProxyConnectionParams());
  DCHECK(!TimerIsRunning());

  next_state_ = STATE_TUNNEL_CONNECT_COMPLETE;
  nested_connect_job_ = std::make_unique<HttpProxyConnectJob>(
      priority(), socket_tag(), common_connect_job_params(),
      params_->GetHttpProxyConnectionParams(), this, &net_log());
  return nested_connect_job_->Connect();
}

int SSLConnectJob::DoTunnelConnectComplete(int result) {
  resolve_error_info_ = nested_connect_job_->GetResolveErrorInfo();
  nested_socket_ = nested_connect_job_->PassSocket();

  if (result < 0) {
    // Extract the information needed to prompt for appropriate proxy
    // authentication so that when ClientSocketPoolBaseHelper calls
    // |GetAdditionalErrorState|, we can easily set the state.
    if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
      ssl_cert_request_info_ = nested_connect_job_->GetCertRequestInfo();
    }
    return result;
  }

  next_state_ = STATE_SSL_CONNECT;
  return result;
}

int SSLConnectJob::DoSSLConnect() {
  TRACE_EVENT0(NetTracingCategory(), "SSLConnectJob::DoSSLConnect");
  DCHECK(!TimerIsRunning());

  next_state_ = STATE_SSL_CONNECT_COMPLETE;

  // Set the timeout to just the time allowed for the SSL handshake.
  ResetTimer(kSSLHandshakeTimeout);

  // Get the transport's connect start and DNS times.
  const LoadTimingInfo::ConnectTiming& socket_connect_timing =
      nested_connect_job_->connect_timing();

  // Overwriting |connect_start| serves two purposes - it adjusts timing so
  // |connect_start| doesn't include dns times, and it adjusts the time so
  // as not to include time spent waiting for an idle socket.
  connect_timing_.connect_start = socket_connect_timing.connect_start;
  connect_timing_.domain_lookup_start =
      socket_connect_timing.domain_lookup_start;
  connect_timing_.domain_lookup_end = socket_connect_timing.domain_lookup_end;

  ssl_negotiation_started_ = true;
  connect_timing_.ssl_start = base::TimeTicks::Now();

  // Save the `HostResolverEndpointResult`. `nested_connect_job_` is destroyed
  // at the end of this function.
  endpoint_result_ = nested_connect_job_->GetHostResolverEndpointResult();

  SSLConfig ssl_config = params_->ssl_config();
  ssl_config.ignore_certificate_errors =
      *common_connect_job_params()->ignore_certificate_errors;
  ssl_config.network_anonymization_key = params_->network_anonymization_key();

  if (ssl_client_context()->config().ech_enabled) {
    if (ech_retry_configs_) {
      ssl_config.ech_config_list = *ech_retry_configs_;
    } else if (endpoint_result_) {
      ssl_config.ech_config_list = endpoint_result_->metadata.ech_config_list;
    }
    if (!ssl_config.ech_config_list.empty()) {
      // Overriding the DNS lookup only works for direct connections. We
      // currently do not support ECH with other connection types.
      DCHECK_EQ(params_->GetConnectionType(), SSLSocketParams::DIRECT);
    }
  }

  ssl_socket_ = client_socket_factory()->CreateSSLClientSocket(
      ssl_client_context(), std::move(nested_socket_), params_->host_and_port(),
      ssl_config);
  nested_connect_job_.reset();
  return ssl_socket_->Connect(callback_);
}

int SSLConnectJob::DoSSLConnectComplete(int result) {
  connect_timing_.ssl_end = base::TimeTicks::Now();

  if (result != OK && !server_address_.address().empty()) {
    connection_attempts_.push_back(ConnectionAttempt(server_address_, result));
    server_address_ = IPEndPoint();
  }

  // Historically, many servers which negotiated SHA-1 server signatures in
  // TLS 1.2 actually support SHA-2 but preferentially sign SHA-1 if available.
  // In order to get accurate metrics while deprecating SHA-1, we initially
  // connected with SHA-1 disabled and then retried with enabled.
  //
  // SHA-1 is now always disabled, but we retained the fallback to separate the
  // effect of disabling SHA-1 from the effect of having a single automatic
  // retry on a potentially unreliably network connection.
  //
  // TODO(crbug.com/40085786): Remove this now redundant retry.
  if (disable_legacy_crypto_with_fallback_ &&
      (result == ERR_CONNECTION_CLOSED || result == ERR_CONNECTION_RESET ||
       result == ERR_SSL_PROTOCOL_ERROR ||
       result == ERR_SSL_VERSION_OR_CIPHER_MISMATCH)) {
    ResetStateForRestart();
    disable_legacy_crypto_with_fallback_ = false;
    next_state_ = GetInitialState(params_->GetConnectionType());
    return OK;
  }

  // We record metrics based on whether the server advertised ECH support in
  // DNS. This allows the metrics to measure the same set of servers in both
  // control and experiment group.
  const bool is_ech_capable =
      endpoint_result_ && !endpoint_result_->metadata.ech_config_list.empty();
  const bool ech_enabled = ssl_client_context()->config().ech_enabled;

  if (!ech_retry_configs_ && result == ERR_ECH_NOT_NEGOTIATED && ech_enabled) {
    // We used ECH, and the server could not decrypt the ClientHello. However,
    // it was able to handshake with the public name and send authenticated
    // retry configs. If this is not the first time around, retry the connection
    // with the new ECHConfigList, or with ECH disabled (empty retry configs),
    // as directed.
    //
    // See
    // https://www.ietf.org/archive/id/draft-ietf-tls-esni-13.html#section-6.1.6
    DCHECK(is_ech_capable);
    ech_retry_configs_ = ssl_socket_->GetECHRetryConfigs();
    net_log().AddEvent(
        NetLogEventType::SSL_CONNECT_JOB_RESTART_WITH_ECH_CONFIG_LIST, [&] {
          return base::Value::Dict().Set(
              "bytes", NetLogBinaryValue(*ech_retry_configs_));
        });

    ResetStateForRestart();
    next_state_ = GetInitialState(params_->GetConnectionType());
    return OK;
  }

  SSLClientSocket::RecordSSLConnectResult(*ssl_socket_, result, is_ech_capable,
                                          ech_enabled, ech_retry_configs_,
                                          connect_timing_);

  if (result == OK || IsCertificateError(result)) {
    SetSocket(std::move(ssl_socket_), std::move(dns_aliases_));
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    ssl_cert_request_info_ = base::MakeRefCounted<SSLCertRequestInfo>();
    ssl_socket_->GetSSLCertRequestInfo(ssl_cert_request_info_.get());
  }

  return result;
}

SSLConnectJob::State SSLConnectJob::GetInitialState(
    SSLSocketParams::ConnectionType connection_type) {
  switch (connection_type) {
    case SSLSocketParams::DIRECT:
      return STATE_TRANSPORT_CONNECT;
    case SSLSocketParams::HTTP_PROXY:
      return STATE_TUNNEL_CONNECT;
    case SSLSocketParams::SOCKS_PROXY:
      return STATE_SOCKS_CONNECT;
  }
  NOTREACHED_IN_MIGRATION();
  return STATE_NONE;
}

int SSLConnectJob::ConnectInternal() {
  next_state_ = GetInitialState(params_->GetConnectionType());
  return DoLoop(OK);
}

void SSLConnectJob::ResetStateForRestart() {
  ResetTimer(base::TimeDelta());
  nested_connect_job_ = nullptr;
  nested_socket_ = nullptr;
  ssl_socket_ = nullptr;
  ssl_cert_request_info_ = nullptr;
  ssl_negotiation_started_ = false;
  resolve_error_info_ = ResolveErrorInfo();
  server_address_ = IPEndPoint();
}

void SSLConnectJob::ChangePriorityInternal(RequestPriority priority) {
  if (nested_connect_job_) {
    nested_connect_job_->ChangePriority(priority);
  }
}

}  // namespace net

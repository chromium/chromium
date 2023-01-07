// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_connect_job.h"

#include <cstdlib>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
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
#include "net/ssl/ssl_legacy_crypto_fallback.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

// Timeout for the SSL handshake portion of the connect.
constexpr base::TimeDelta kSSLHandshakeTimeout(base::Seconds(30));

}  // namespace

SSLSocketParams::SSLSocketParams(
    scoped_refptr<TransportSocketParams> direct_params,
    scoped_refptr<SOCKSSocketParams> socks_proxy_params,
    scoped_refptr<HttpProxySocketParams> http_proxy_params,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    PrivacyMode privacy_mode,
    NetworkAnonymizationKey network_anonymization_key)
    : direct_params_(std::move(direct_params)),
      socks_proxy_params_(std::move(socks_proxy_params)),
      http_proxy_params_(std::move(http_proxy_params)),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      privacy_mode_(privacy_mode),
      network_anonymization_key_(network_anonymization_key) {
  // Only one set of lower level ConnectJob params should be non-NULL.
  DCHECK((direct_params_ && !socks_proxy_params_ && !http_proxy_params_) ||
         (!direct_params_ && socks_proxy_params_ && !http_proxy_params_) ||
         (!direct_params_ && !socks_proxy_params_ && http_proxy_params_));
}

SSLSocketParams::~SSLSocketParams() = default;

SSLSocketParams::ConnectionType SSLSocketParams::GetConnectionType() const {
  if (direct_params_.get()) {
    DCHECK(!socks_proxy_params_.get());
    DCHECK(!http_proxy_params_.get());
    return DIRECT;
  }

  if (socks_proxy_params_.get()) {
    DCHECK(!http_proxy_params_.get());
    return SOCKS_PROXY;
  }

  DCHECK(http_proxy_params_.get());
  return HTTP_PROXY;
}

const scoped_refptr<TransportSocketParams>&
SSLSocketParams::GetDirectConnectionParams() const {
  DCHECK_EQ(GetConnectionType(), DIRECT);
  return direct_params_;
}

const scoped_refptr<SOCKSSocketParams>&
SSLSocketParams::GetSocksProxyConnectionParams() const {
  DCHECK_EQ(GetConnectionType(), SOCKS_PROXY);
  return socks_proxy_params_;
}

const scoped_refptr<HttpProxySocketParams>&
SSLSocketParams::GetHttpProxyConnectionParams() const {
  DCHECK_EQ(GetConnectionType(), HTTP_PROXY);
  return http_proxy_params_;
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
      if (nested_socket_)
        return LOAD_STATE_ESTABLISHING_PROXY_TUNNEL;
      return nested_connect_job_->GetLoadState();
    case STATE_SSL_CONNECT:
    case STATE_SSL_CONNECT_COMPLETE:
      return LOAD_STATE_SSL_HANDSHAKE;
    default:
      NOTREACHED();
      return LOAD_STATE_IDLE;
  }
}

bool SSLConnectJob::HasEstablishedConnection() const {
  // If waiting on a nested ConnectJob, defer to that ConnectJob's state.
  if (nested_connect_job_)
    return nested_connect_job_->HasEstablishedConnection();
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
  if (rv != ERR_IO_PENDING)
    NotifyDelegateOfCompletion(rv);  // Deletes |this|.
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
        NOTREACHED() << "bad state";
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
  absl::optional<TransportConnectJob::EndpointResultOverride>
      endpoint_result_override;
  if (ech_retry_configs_) {
    DCHECK(ssl_client_context()->EncryptedClientHelloEnabled());
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
  scoped_refptr<HttpProxySocketParams> http_proxy_params =
      params_->GetHttpProxyConnectionParams();
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
  ssl_config.network_anonymization_key = params_->network_anonymization_key();
  ssl_config.privacy_mode = params_->privacy_mode();
  ssl_config.disable_legacy_crypto = disable_legacy_crypto_with_fallback_;

  if (ssl_client_context()->EncryptedClientHelloEnabled()) {
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

  // Many servers which negotiate SHA-1 server signatures in TLS 1.2 actually
  // support SHA-2 but preferentially sign SHA-1 if available.
  //
  // To get more accurate metrics, initially connect with SHA-1 disabled. If
  // this fails, retry with them enabled. This keeps the legacy algorithms
  // working for now, but they will only appear in metrics and DevTools if the
  // site relies on them.
  //
  // See https://crbug.com/658905.
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

  if (!ech_retry_configs_ && result == ERR_ECH_NOT_NEGOTIATED &&
      ssl_client_context()->EncryptedClientHelloEnabled()) {
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
          base::Value::Dict dict;
          dict.Set("bytes", NetLogBinaryValue(*ech_retry_configs_));
          return base::Value(std::move(dict));
        });

    // TODO(https://crbug.com/1091403): Add histograms for how often this
    // happens.
    ResetStateForRestart();
    next_state_ = GetInitialState(params_->GetConnectionType());
    return OK;
  }

  const std::string& host = params_->host_and_port().host();
  if (is_ech_capable &&
      base::FeatureList::IsEnabled(features::kEncryptedClientHello)) {
    // These values are persisted to logs. Entries should not be renumbered
    // and numeric values should never be reused.
    enum class ECHResult {
      // The connection succeeded on the initial connection.
      kSuccessInitial = 0,
      // The connection failed on the initial connection, without providing
      // retry configs.
      kErrorInitial = 1,
      // The connection succeeded after getting retry configs.
      kSuccessRetry = 2,
      // The connection failed after getting retry configs.
      kErrorRetry = 3,
      // The connection succeeded after getting a rollback signal.
      kSuccessRollback = 4,
      // The connection failed after getting a rollback signal.
      kErrorRollback = 5,
      kMaxValue = kErrorRollback,
    };
    const bool is_ok = result == OK;
    ECHResult ech_result;
    if (!ech_retry_configs_.has_value()) {
      ech_result =
          is_ok ? ECHResult::kSuccessInitial : ECHResult::kErrorInitial;
    } else if (ech_retry_configs_->empty()) {
      ech_result =
          is_ok ? ECHResult::kSuccessRollback : ECHResult::kErrorRollback;
    } else {
      ech_result = is_ok ? ECHResult::kSuccessRetry : ECHResult::kErrorRetry;
    }
    base::UmaHistogramEnumeration("Net.SSL.ECHResult", ech_result);
  }

  if (result == OK) {
    DCHECK(!connect_timing_.ssl_start.is_null());
    base::TimeDelta connect_duration =
        connect_timing_.ssl_end - connect_timing_.ssl_start;
    UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency_2", connect_duration,
                               base::Milliseconds(1), base::Minutes(1), 100);
    if (is_ech_capable) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency_ECH",
                                 connect_duration, base::Milliseconds(1),
                                 base::Minutes(1), 100);
    }

    SSLInfo ssl_info;
    bool has_ssl_info = ssl_socket_->GetSSLInfo(&ssl_info);
    DCHECK(has_ssl_info);

    SSLVersion version =
        SSLConnectionStatusToVersion(ssl_info.connection_status);
    UMA_HISTOGRAM_ENUMERATION("Net.SSLVersion", version,
                              SSL_CONNECTION_VERSION_MAX);
    if (IsGoogleHost(host)) {
      // Google hosts all support TLS 1.2, so any occurrences of TLS 1.0 or TLS
      // 1.1 will be from an outdated insecure TLS MITM proxy, such as some
      // antivirus configurations. TLS 1.0 and 1.1 are deprecated, so record
      // these to see how prevalent they are. See https://crbug.com/896013.
      UMA_HISTOGRAM_ENUMERATION("Net.SSLVersionGoogle", version,
                                SSL_CONNECTION_VERSION_MAX);
    }

    uint16_t cipher_suite =
        SSLConnectionStatusToCipherSuite(ssl_info.connection_status);
    base::UmaHistogramSparse("Net.SSL_CipherSuite", cipher_suite);

    if (ssl_info.key_exchange_group != 0) {
      base::UmaHistogramSparse("Net.SSL_KeyExchange.ECDHE",
                               ssl_info.key_exchange_group);
    }

    // Classify whether the connection required the legacy crypto fallback.
    SSLLegacyCryptoFallback fallback = SSLLegacyCryptoFallback::kNoFallback;
    if (!disable_legacy_crypto_with_fallback_) {
      // Some servers, though they do not negotiate SHA-1, still fail the
      // connection when SHA-1 is not offered. We believe these are servers
      // which match the sent certificates against the ClientHello and then
      // are configured with a SHA-1 certificate.
      //
      // SHA-1 certificate chains are no longer accepted, however servers may
      // send extra unused certificates, most commonly a copy of the trust
      // anchor. We only need to check for RSASSA-PKCS1-v1_5 signatures, because
      // other SHA-1 signature types have already been removed from the
      // ClientHello.
      bool sent_sha1_cert = ssl_info.unverified_cert &&
                            x509_util::HasRsaPkcs1Sha1Signature(
                                ssl_info.unverified_cert->cert_buffer());
      if (!sent_sha1_cert && ssl_info.unverified_cert) {
        for (const auto& cert :
             ssl_info.unverified_cert->intermediate_buffers()) {
          if (x509_util::HasRsaPkcs1Sha1Signature(cert.get())) {
            sent_sha1_cert = true;
            break;
          }
        }
      }
      if (ssl_info.peer_signature_algorithm == SSL_SIGN_RSA_PKCS1_SHA1) {
        fallback = sent_sha1_cert
                       ? SSLLegacyCryptoFallback::kSentSHA1CertAndUsedSHA1
                       : SSLLegacyCryptoFallback::kUsedSHA1;
      } else {
        fallback = sent_sha1_cert ? SSLLegacyCryptoFallback::kSentSHA1Cert
                                  : SSLLegacyCryptoFallback::kUnknownReason;
      }
    }
    UMA_HISTOGRAM_ENUMERATION("Net.SSLLegacyCryptoFallback2", fallback);
  }

  base::UmaHistogramSparse("Net.SSL_Connection_Error", std::abs(result));
  if (is_ech_capable) {
    base::UmaHistogramSparse("Net.SSL_Connection_Error_ECH", std::abs(result));
  }

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
  NOTREACHED();
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
  if (nested_connect_job_)
    nested_connect_job_->ChangePriority(priority);
}

}  // namespace net

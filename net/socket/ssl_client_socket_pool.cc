// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket_pool.h"

#include <cstdlib>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/base/url_util.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

class NetLog;

SSLSocketParams::SSLSocketParams(
    const scoped_refptr<TransportSocketParams>& direct_params,
    const scoped_refptr<SOCKSSocketParams>& socks_proxy_params,
    const scoped_refptr<HttpProxySocketParams>& http_proxy_params,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    PrivacyMode privacy_mode,
    bool ignore_certificate_errors)
    : direct_params_(direct_params),
      socks_proxy_params_(socks_proxy_params),
      http_proxy_params_(http_proxy_params),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      privacy_mode_(privacy_mode),
      ignore_certificate_errors_(ignore_certificate_errors) {
  // Only one set of lower level pool params should be non-NULL.
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

// Timeout for the SSL handshake portion of the connect.
static const int kSSLHandshakeTimeoutInSeconds = 30;

SSLConnectJob::SSLConnectJob(const std::string& group_name,
                             RequestPriority priority,
                             const SocketTag& socket_tag,
                             ClientSocketPool::RespectLimits respect_limits,
                             const scoped_refptr<SSLSocketParams>& params,
                             const base::TimeDelta& timeout_duration,
                             TransportClientSocketPool* transport_pool,
                             SOCKSClientSocketPool* socks_pool,
                             HttpProxyClientSocketPool* http_proxy_pool,
                             ClientSocketFactory* client_socket_factory,
                             const SSLClientSocketContext& context,
                             Delegate* delegate,
                             NetLog* net_log)
    : ConnectJob(
          group_name,
          timeout_duration,
          priority,
          socket_tag,
          respect_limits,
          delegate,
          NetLogWithSource::Make(net_log, NetLogSourceType::SSL_CONNECT_JOB)),
      params_(params),
      transport_pool_(transport_pool),
      socks_pool_(socks_pool),
      http_proxy_pool_(http_proxy_pool),
      client_socket_factory_(client_socket_factory),
      context_(context.cert_verifier,
               context.channel_id_service,
               context.transport_security_state,
               context.cert_transparency_verifier,
               context.ct_policy_enforcer,
               (context.ssl_session_cache_shard.empty()
                    ? context.ssl_session_cache_shard
                    : (params->privacy_mode() == PRIVACY_MODE_ENABLED
                           ? "pm/" + context.ssl_session_cache_shard
                           : context.ssl_session_cache_shard))),
      callback_(base::BindRepeating(&SSLConnectJob::OnIOComplete,
                                    base::Unretained(this))) {}

SSLConnectJob::~SSLConnectJob() = default;

LoadState SSLConnectJob::GetLoadState() const {
  switch (next_state_) {
    case STATE_TUNNEL_CONNECT_COMPLETE:
      if (transport_socket_handle_->socket())
        return LOAD_STATE_ESTABLISHING_PROXY_TUNNEL;
      FALLTHROUGH;
    case STATE_TRANSPORT_CONNECT:
    case STATE_TRANSPORT_CONNECT_COMPLETE:
    case STATE_SOCKS_CONNECT:
    case STATE_SOCKS_CONNECT_COMPLETE:
    case STATE_TUNNEL_CONNECT:
      return transport_socket_handle_->GetLoadState();
    case STATE_SSL_CONNECT:
    case STATE_SSL_CONNECT_COMPLETE:
      return LOAD_STATE_SSL_HANDSHAKE;
    default:
      NOTREACHED();
      return LOAD_STATE_IDLE;
  }
}

void SSLConnectJob::GetAdditionalErrorState(ClientSocketHandle* handle) {
  // Headers in |error_response_info_| indicate a proxy tunnel setup
  // problem. See DoTunnelConnectComplete.
  if (error_response_info_.headers.get()) {
    handle->set_pending_http_proxy_connection(
        std::move(transport_socket_handle_));
  }
  handle->set_ssl_error_response_info(error_response_info_);
  if (!connect_timing_.ssl_start.is_null())
    handle->set_is_ssl_error(true);

  handle->set_connection_attempts(connection_attempts_);
}

void SSLConnectJob::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    NotifyDelegateOfCompletion(rv);  // Deletes |this|.
}

int SSLConnectJob::DoLoop(int result) {
  TRACE_EVENT0(kNetTracingCategory, "SSLConnectJob::DoLoop");
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
  DCHECK(transport_pool_);

  next_state_ = STATE_TRANSPORT_CONNECT_COMPLETE;
  transport_socket_handle_.reset(new ClientSocketHandle());
  scoped_refptr<TransportSocketParams> direct_params =
      params_->GetDirectConnectionParams();
  return transport_socket_handle_->Init(group_name(), direct_params, priority(),
                                        socket_tag(), respect_limits(),
                                        callback_, transport_pool_, net_log());
}

int SSLConnectJob::DoTransportConnectComplete(int result) {
  connection_attempts_.insert(
      connection_attempts_.end(),
      transport_socket_handle_->connection_attempts().begin(),
      transport_socket_handle_->connection_attempts().end());
  if (result == OK) {
    next_state_ = STATE_SSL_CONNECT;
    transport_socket_handle_->socket()->GetPeerAddress(&server_address_);
  }

  return result;
}

int SSLConnectJob::DoSOCKSConnect() {
  DCHECK(socks_pool_);
  next_state_ = STATE_SOCKS_CONNECT_COMPLETE;
  transport_socket_handle_.reset(new ClientSocketHandle());
  scoped_refptr<SOCKSSocketParams> socks_proxy_params =
      params_->GetSocksProxyConnectionParams();
  return transport_socket_handle_->Init(
      group_name(), socks_proxy_params, priority(), socket_tag(),
      respect_limits(), callback_, socks_pool_, net_log());
}

int SSLConnectJob::DoSOCKSConnectComplete(int result) {
  if (result == OK)
    next_state_ = STATE_SSL_CONNECT;

  return result;
}

int SSLConnectJob::DoTunnelConnect() {
  DCHECK(http_proxy_pool_);
  next_state_ = STATE_TUNNEL_CONNECT_COMPLETE;

  transport_socket_handle_.reset(new ClientSocketHandle());
  scoped_refptr<HttpProxySocketParams> http_proxy_params =
      params_->GetHttpProxyConnectionParams();
  return transport_socket_handle_->Init(
      group_name(), http_proxy_params, priority(), socket_tag(),
      respect_limits(), callback_, http_proxy_pool_, net_log());
}

int SSLConnectJob::DoTunnelConnectComplete(int result) {
  // Extract the information needed to prompt for appropriate proxy
  // authentication so that when ClientSocketPoolBaseHelper calls
  // |GetAdditionalErrorState|, we can easily set the state.
  if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    error_response_info_ = transport_socket_handle_->ssl_error_response_info();
  } else if (result == ERR_PROXY_AUTH_REQUESTED ||
             result == ERR_HTTPS_PROXY_TUNNEL_RESPONSE) {
    StreamSocket* socket = transport_socket_handle_->socket();
    ProxyClientSocket* tunnel_socket = static_cast<ProxyClientSocket*>(socket);
    error_response_info_ = *tunnel_socket->GetConnectResponseInfo();
  }
  if (result < 0)
    return result;

  next_state_ = STATE_SSL_CONNECT;
  return result;
}

int SSLConnectJob::DoSSLConnect() {
  TRACE_EVENT0(kNetTracingCategory, "SSLConnectJob::DoSSLConnect");
  next_state_ = STATE_SSL_CONNECT_COMPLETE;

  // Reset the timeout to just the time allowed for the SSL handshake.
  ResetTimer(base::TimeDelta::FromSeconds(kSSLHandshakeTimeoutInSeconds));

  // If the handle has a fresh socket, get its connect start and DNS times.
  // This should always be the case.
  const LoadTimingInfo::ConnectTiming& socket_connect_timing =
      transport_socket_handle_->connect_timing();
  if (!transport_socket_handle_->is_reused() &&
      !socket_connect_timing.connect_start.is_null()) {
    // Overwriting |connect_start| serves two purposes - it adjusts timing so
    // |connect_start| doesn't include dns times, and it adjusts the time so
    // as not to include time spent waiting for an idle socket.
    connect_timing_.connect_start = socket_connect_timing.connect_start;
    connect_timing_.dns_start = socket_connect_timing.dns_start;
    connect_timing_.dns_end = socket_connect_timing.dns_end;
  }

  connect_timing_.ssl_start = base::TimeTicks::Now();

  ssl_socket_ = client_socket_factory_->CreateSSLClientSocket(
      std::move(transport_socket_handle_), params_->host_and_port(),
      params_->ssl_config(), context_);
  return ssl_socket_->Connect(callback_);
}

int SSLConnectJob::DoSSLConnectComplete(int result) {
  connect_timing_.ssl_end = base::TimeTicks::Now();

  if (result != OK && !server_address_.address().empty()) {
    connection_attempts_.push_back(ConnectionAttempt(server_address_, result));
    server_address_ = IPEndPoint();
  }

  const std::string& host = params_->host_and_port().host();
  bool tls13_supported = IsTLS13ExperimentHost(host);

  if (result == OK ||
      (params_->ignore_certificate_errors() && IsCertificateError(result))) {
    DCHECK(!connect_timing_.ssl_start.is_null());
    base::TimeDelta connect_duration =
        connect_timing_.ssl_end - connect_timing_.ssl_start;
    UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency_2",
                               connect_duration,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMinutes(1),
                               100);

    SSLInfo ssl_info;
    bool has_ssl_info = ssl_socket_->GetSSLInfo(&ssl_info);
    DCHECK(has_ssl_info);

    UMA_HISTOGRAM_ENUMERATION("Net.SSLVersion", SSLConnectionStatusToVersion(
                                                    ssl_info.connection_status),
                              SSL_CONNECTION_VERSION_MAX);

    uint16_t cipher_suite =
        SSLConnectionStatusToCipherSuite(ssl_info.connection_status);
    base::UmaHistogramSparse("Net.SSL_CipherSuite", cipher_suite);

    if (ssl_info.key_exchange_group != 0) {
      base::UmaHistogramSparse("Net.SSL_KeyExchange.ECDHE",
                               ssl_info.key_exchange_group);
    }

    if (ssl_info.handshake_type == SSLInfo::HANDSHAKE_RESUME) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency_Resume_Handshake",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(1),
                                 100);
    } else if (ssl_info.handshake_type == SSLInfo::HANDSHAKE_FULL) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency_Full_Handshake",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(1),
                                 100);
    }

    if (tls13_supported) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSL_Connection_Latency_TLS13Experiment",
                                 connect_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(1), 100);
    }
  }

  // Don't double-count the version interference probes.
  if (!params_->ssl_config().version_interference_probe) {
    base::UmaHistogramSparse("Net.SSL_Connection_Error", std::abs(result));

    if (tls13_supported) {
      base::UmaHistogramSparse("Net.SSL_Connection_Error_TLS13Experiment",
                               std::abs(result));
    }
  }

  if (result == OK || IsCertificateError(result)) {
    SetSocket(std::move(ssl_socket_));
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    error_response_info_.cert_request_info = new SSLCertRequestInfo;
    ssl_socket_->GetSSLCertRequestInfo(
        error_response_info_.cert_request_info.get());
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

SSLClientSocketPool::SSLConnectJobFactory::SSLConnectJobFactory(
    TransportClientSocketPool* transport_pool,
    SOCKSClientSocketPool* socks_pool,
    HttpProxyClientSocketPool* http_proxy_pool,
    ClientSocketFactory* client_socket_factory,
    const SSLClientSocketContext& context,
    NetLog* net_log)
    : transport_pool_(transport_pool),
      socks_pool_(socks_pool),
      http_proxy_pool_(http_proxy_pool),
      client_socket_factory_(client_socket_factory),
      context_(context),
      net_log_(net_log) {
  base::TimeDelta max_transport_timeout = base::TimeDelta();
  base::TimeDelta pool_timeout;
  if (transport_pool_)
    max_transport_timeout = transport_pool_->ConnectionTimeout();
  if (socks_pool_) {
    pool_timeout = socks_pool_->ConnectionTimeout();
    if (pool_timeout > max_transport_timeout)
      max_transport_timeout = pool_timeout;
  }
  if (http_proxy_pool_) {
    pool_timeout = http_proxy_pool_->ConnectionTimeout();
    if (pool_timeout > max_transport_timeout)
      max_transport_timeout = pool_timeout;
  }
  timeout_ = max_transport_timeout +
      base::TimeDelta::FromSeconds(kSSLHandshakeTimeoutInSeconds);
}

SSLClientSocketPool::SSLConnectJobFactory::~SSLConnectJobFactory() = default;

SSLClientSocketPool::SSLClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    CertVerifier* cert_verifier,
    ChannelIDService* channel_id_service,
    TransportSecurityState* transport_security_state,
    CTVerifier* cert_transparency_verifier,
    CTPolicyEnforcer* ct_policy_enforcer,
    const std::string& ssl_session_cache_shard,
    ClientSocketFactory* client_socket_factory,
    TransportClientSocketPool* transport_pool,
    SOCKSClientSocketPool* socks_pool,
    HttpProxyClientSocketPool* http_proxy_pool,
    SSLConfigService* ssl_config_service,
    NetLog* net_log)
    : transport_pool_(transport_pool),
      socks_pool_(socks_pool),
      http_proxy_pool_(http_proxy_pool),
      base_(this,
            max_sockets,
            max_sockets_per_group,
            ClientSocketPool::unused_idle_socket_timeout(),
            ClientSocketPool::used_idle_socket_timeout(),
            new SSLConnectJobFactory(
                transport_pool,
                socks_pool,
                http_proxy_pool,
                client_socket_factory,
                SSLClientSocketContext(cert_verifier,
                                       channel_id_service,
                                       transport_security_state,
                                       cert_transparency_verifier,
                                       ct_policy_enforcer,
                                       ssl_session_cache_shard),
                net_log)),
      ssl_config_service_(ssl_config_service) {
  if (ssl_config_service_)
    ssl_config_service_->AddObserver(this);
  if (transport_pool_)
    base_.AddLowerLayeredPool(transport_pool_);
  if (socks_pool_)
    base_.AddLowerLayeredPool(socks_pool_);
  if (http_proxy_pool_)
    base_.AddLowerLayeredPool(http_proxy_pool_);
}

SSLClientSocketPool::~SSLClientSocketPool() {
  if (ssl_config_service_)
    ssl_config_service_->RemoveObserver(this);
}

std::unique_ptr<ConnectJob>
SSLClientSocketPool::SSLConnectJobFactory::NewConnectJob(
    const std::string& group_name,
    const PoolBase::Request& request,
    ConnectJob::Delegate* delegate) const {
  return std::unique_ptr<ConnectJob>(new SSLConnectJob(
      group_name, request.priority(), request.socket_tag(),
      request.respect_limits(), request.params(), ConnectionTimeout(),
      transport_pool_, socks_pool_, http_proxy_pool_, client_socket_factory_,
      context_, delegate, net_log_));
}

base::TimeDelta SSLClientSocketPool::SSLConnectJobFactory::ConnectionTimeout()
    const {
  return timeout_;
}

int SSLClientSocketPool::RequestSocket(const std::string& group_name,
                                       const void* socket_params,
                                       RequestPriority priority,
                                       const SocketTag& socket_tag,
                                       RespectLimits respect_limits,
                                       ClientSocketHandle* handle,
                                       CompletionOnceCallback callback,
                                       const NetLogWithSource& net_log) {
  const scoped_refptr<SSLSocketParams>* casted_socket_params =
      static_cast<const scoped_refptr<SSLSocketParams>*>(socket_params);

  return base_.RequestSocket(group_name, *casted_socket_params, priority,
                             socket_tag, respect_limits, handle,
                             std::move(callback), net_log);
}

void SSLClientSocketPool::RequestSockets(const std::string& group_name,
                                         const void* params,
                                         int num_sockets,
                                         const NetLogWithSource& net_log) {
  const scoped_refptr<SSLSocketParams>* casted_params =
      static_cast<const scoped_refptr<SSLSocketParams>*>(params);

  base_.RequestSockets(group_name, *casted_params, num_sockets, net_log);
}

void SSLClientSocketPool::SetPriority(const std::string& group_name,
                                      ClientSocketHandle* handle,
                                      RequestPriority priority) {
  base_.SetPriority(group_name, handle, priority);
}

void SSLClientSocketPool::CancelRequest(const std::string& group_name,
                                        ClientSocketHandle* handle) {
  base_.CancelRequest(group_name, handle);
}

void SSLClientSocketPool::ReleaseSocket(const std::string& group_name,
                                        std::unique_ptr<StreamSocket> socket,
                                        int id) {
  base_.ReleaseSocket(group_name, std::move(socket), id);
}

void SSLClientSocketPool::FlushWithError(int error) {
  base_.FlushWithError(error);
}

void SSLClientSocketPool::CloseIdleSockets() {
  base_.CloseIdleSockets();
}

void SSLClientSocketPool::CloseIdleSocketsInGroup(
    const std::string& group_name) {
  base_.CloseIdleSocketsInGroup(group_name);
}

int SSLClientSocketPool::IdleSocketCount() const {
  return base_.idle_socket_count();
}

int SSLClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return base_.IdleSocketCountInGroup(group_name);
}

LoadState SSLClientSocketPool::GetLoadState(
    const std::string& group_name, const ClientSocketHandle* handle) const {
  return base_.GetLoadState(group_name, handle);
}

void SSLClientSocketPool::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_dump_absolute_name) const {
  base_.DumpMemoryStats(pmd, parent_dump_absolute_name);
}

std::unique_ptr<base::DictionaryValue> SSLClientSocketPool::GetInfoAsValue(
    const std::string& name,
    const std::string& type,
    bool include_nested_pools) const {
  std::unique_ptr<base::DictionaryValue> dict(base_.GetInfoAsValue(name, type));
  if (include_nested_pools) {
    auto list = std::make_unique<base::ListValue>();
    if (transport_pool_) {
      list->Append(transport_pool_->GetInfoAsValue("transport_socket_pool",
                                                   "transport_socket_pool",
                                                   false));
    }
    if (socks_pool_) {
      list->Append(socks_pool_->GetInfoAsValue("socks_pool",
                                               "socks_pool",
                                               true));
    }
    if (http_proxy_pool_) {
      list->Append(http_proxy_pool_->GetInfoAsValue("http_proxy_pool",
                                                    "http_proxy_pool",
                                                    true));
    }
    dict->Set("nested_pools", std::move(list));
  }
  return dict;
}

base::TimeDelta SSLClientSocketPool::ConnectionTimeout() const {
  return base_.ConnectionTimeout();
}

bool SSLClientSocketPool::IsStalled() const {
  return base_.IsStalled();
}

void SSLClientSocketPool::AddHigherLayeredPool(HigherLayeredPool* higher_pool) {
  base_.AddHigherLayeredPool(higher_pool);
}

void SSLClientSocketPool::RemoveHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  base_.RemoveHigherLayeredPool(higher_pool);
}

bool SSLClientSocketPool::CloseOneIdleConnection() {
  if (base_.CloseOneIdleSocket())
    return true;
  return base_.CloseOneIdleConnectionInHigherLayeredPool();
}

void SSLClientSocketPool::OnSSLConfigChanged() {
  FlushWithError(ERR_NETWORK_CHANGED);
}

}  // namespace net

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/connect_job.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/stream_socket.h"
#include "net/socket/transport_connect_job.h"
#include "net/ssl/ssl_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

CommonConnectJobParams::CommonConnectJobParams(
    ClientSocketFactory* client_socket_factory,
    HostResolver* host_resolver,
    HttpAuthCache* http_auth_cache,
    HttpAuthHandlerFactory* http_auth_handler_factory,
    SpdySessionPool* spdy_session_pool,
    const quic::ParsedQuicVersionVector* quic_supported_versions,
    QuicStreamFactory* quic_stream_factory,
    ProxyDelegate* proxy_delegate,
    const HttpUserAgentSettings* http_user_agent_settings,
    SSLClientContext* ssl_client_context,
    SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
    NetworkQualityEstimator* network_quality_estimator,
    NetLog* net_log,
    WebSocketEndpointLockManager* websocket_endpoint_lock_manager)
    : client_socket_factory(client_socket_factory),
      host_resolver(host_resolver),
      http_auth_cache(http_auth_cache),
      http_auth_handler_factory(http_auth_handler_factory),
      spdy_session_pool(spdy_session_pool),
      quic_supported_versions(quic_supported_versions),
      quic_stream_factory(quic_stream_factory),
      proxy_delegate(proxy_delegate),
      http_user_agent_settings(http_user_agent_settings),
      ssl_client_context(ssl_client_context),
      socket_performance_watcher_factory(socket_performance_watcher_factory),
      network_quality_estimator(network_quality_estimator),
      net_log(net_log),
      websocket_endpoint_lock_manager(websocket_endpoint_lock_manager) {}

CommonConnectJobParams::CommonConnectJobParams(
    const CommonConnectJobParams& other) = default;

CommonConnectJobParams::~CommonConnectJobParams() = default;

CommonConnectJobParams& CommonConnectJobParams::operator=(
    const CommonConnectJobParams& other) = default;

ConnectJob::ConnectJob(RequestPriority priority,
                       const SocketTag& socket_tag,
                       base::TimeDelta timeout_duration,
                       const CommonConnectJobParams* common_connect_job_params,
                       Delegate* delegate,
                       const NetLogWithSource* net_log,
                       NetLogSourceType net_log_source_type,
                       NetLogEventType net_log_connect_event_type)
    : timeout_duration_(timeout_duration),
      priority_(priority),
      socket_tag_(socket_tag),
      common_connect_job_params_(common_connect_job_params),
      delegate_(delegate),
      top_level_job_(net_log == nullptr),
      net_log_(net_log
                   ? *net_log
                   : NetLogWithSource::Make(common_connect_job_params->net_log,
                                            net_log_source_type)),
      net_log_connect_event_type_(net_log_connect_event_type) {
  DCHECK(delegate);
  if (top_level_job_)
    net_log_.BeginEvent(NetLogEventType::CONNECT_JOB);
}

ConnectJob::~ConnectJob() {
  // Log end of Connect event if ConnectJob was still in-progress when
  // destroyed.
  if (delegate_)
    LogConnectCompletion(ERR_ABORTED);
  if (top_level_job_)
    net_log().EndEvent(NetLogEventType::CONNECT_JOB);
}

std::unique_ptr<ConnectJob> ConnectJob::CreateConnectJob(
    bool using_ssl,
    const HostPortPair& endpoint,
    const ProxyServer& proxy_server,
    const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    const SSLConfig* ssl_config_for_origin,
    const SSLConfig* ssl_config_for_proxy,
    bool force_tunnel,
    PrivacyMode privacy_mode,
    const OnHostResolutionCallback& resolution_callback,
    RequestPriority request_priority,
    SocketTag socket_tag,
    const NetworkIsolationKey& network_isolation_key,
    bool disable_secure_dns,
    const CommonConnectJobParams* common_connect_job_params,
    ConnectJob::Delegate* delegate) {
  scoped_refptr<HttpProxySocketParams> http_proxy_params;
  scoped_refptr<SOCKSSocketParams> socks_params;

  if (!proxy_server.is_direct()) {
    // No need to use a NetworkIsolationKey for looking up the proxy's IP
    // address. Cached proxy IP addresses doesn't really expose useful
    // information to destination sites, and not caching them has a performance
    // cost.
    auto proxy_tcp_params = base::MakeRefCounted<TransportSocketParams>(
        proxy_server.host_port_pair(), NetworkIsolationKey(),
        disable_secure_dns, resolution_callback);

    if (proxy_server.is_http() || proxy_server.is_https() ||
        proxy_server.is_quic()) {
      scoped_refptr<SSLSocketParams> ssl_params;
      if (!proxy_server.is_http()) {
        DCHECK(ssl_config_for_proxy);
        // Set ssl_params, and unset proxy_tcp_params
        ssl_params = base::MakeRefCounted<SSLSocketParams>(
            std::move(proxy_tcp_params), nullptr, nullptr,
            proxy_server.host_port_pair(), *ssl_config_for_proxy,
            PRIVACY_MODE_DISABLED, network_isolation_key);
        proxy_tcp_params = nullptr;
      }

      http_proxy_params = base::MakeRefCounted<HttpProxySocketParams>(
          std::move(proxy_tcp_params), std::move(ssl_params),
          proxy_server.is_quic(), endpoint, proxy_server.is_trusted_proxy(),
          force_tunnel || using_ssl, *proxy_annotation_tag,
          network_isolation_key);
    } else {
      DCHECK(proxy_server.is_socks());
      socks_params = base::MakeRefCounted<SOCKSSocketParams>(
          std::move(proxy_tcp_params),
          proxy_server.scheme() == ProxyServer::SCHEME_SOCKS5, endpoint,
          network_isolation_key, *proxy_annotation_tag);
    }
  }

  // Deal with SSL - which layers on top of any given proxy.
  if (using_ssl) {
    DCHECK(ssl_config_for_origin);
    scoped_refptr<TransportSocketParams> ssl_tcp_params;
    if (proxy_server.is_direct()) {
      ssl_tcp_params = base::MakeRefCounted<TransportSocketParams>(
          endpoint, network_isolation_key, disable_secure_dns,
          resolution_callback);
    }
    auto ssl_params = base::MakeRefCounted<SSLSocketParams>(
        std::move(ssl_tcp_params), std::move(socks_params),
        std::move(http_proxy_params), endpoint, *ssl_config_for_origin,
        privacy_mode, network_isolation_key);
    return std::make_unique<SSLConnectJob>(
        request_priority, socket_tag, common_connect_job_params,
        std::move(ssl_params), delegate, nullptr /* net_log */);
  }

  if (proxy_server.is_http() || proxy_server.is_https()) {
    return std::make_unique<HttpProxyConnectJob>(
        request_priority, socket_tag, common_connect_job_params,
        std::move(http_proxy_params), delegate, nullptr /* net_log */);
  }

  if (proxy_server.is_socks()) {
    return std::make_unique<SOCKSConnectJob>(
        request_priority, socket_tag, common_connect_job_params,
        std::move(socks_params), delegate, nullptr /* net_log */);
  }

  DCHECK(proxy_server.is_direct());
  auto tcp_params = base::MakeRefCounted<TransportSocketParams>(
      endpoint, network_isolation_key, disable_secure_dns, resolution_callback);
  return TransportConnectJob::CreateTransportConnectJob(
      std::move(tcp_params), request_priority, socket_tag,
      common_connect_job_params, delegate, nullptr /* net_log */);
}

std::unique_ptr<StreamSocket> ConnectJob::PassSocket() {
  return std::move(socket_);
}

void ConnectJob::ChangePriority(RequestPriority priority) {
  priority_ = priority;
  ChangePriorityInternal(priority);
}

int ConnectJob::Connect() {
  if (!timeout_duration_.is_zero())
    timer_.Start(FROM_HERE, timeout_duration_, this, &ConnectJob::OnTimeout);

  LogConnectStart();

  int rv = ConnectInternal();

  if (rv != ERR_IO_PENDING) {
    LogConnectCompletion(rv);
    delegate_ = nullptr;
  }

  return rv;
}

ConnectionAttempts ConnectJob::GetConnectionAttempts() const {
  // Return empty list by default - used by proxy classes.
  return ConnectionAttempts();
}

bool ConnectJob::IsSSLError() const {
  return false;
}

scoped_refptr<SSLCertRequestInfo> ConnectJob::GetCertRequestInfo() {
  return nullptr;
}

void ConnectJob::SetSocket(std::unique_ptr<StreamSocket> socket) {
  if (socket)
    net_log().AddEvent(NetLogEventType::CONNECT_JOB_SET_SOCKET);
  socket_ = std::move(socket);
}

void ConnectJob::NotifyDelegateOfCompletion(int rv) {
  TRACE_EVENT0(NetTracingCategory(), "ConnectJob::NotifyDelegateOfCompletion");
  // The delegate will own |this|.
  Delegate* delegate = delegate_;
  delegate_ = nullptr;

  LogConnectCompletion(rv);
  delegate->OnConnectJobComplete(rv, this);
}

void ConnectJob::NotifyDelegateOfProxyAuth(
    const HttpResponseInfo& response,
    HttpAuthController* auth_controller,
    base::OnceClosure restart_with_auth_callback) {
  delegate_->OnNeedsProxyAuth(response, auth_controller,
                              std::move(restart_with_auth_callback), this);
}

void ConnectJob::ResetTimer(base::TimeDelta remaining_time) {
  timer_.Stop();
  if (!remaining_time.is_zero())
    timer_.Start(FROM_HERE, remaining_time, this, &ConnectJob::OnTimeout);
}

bool ConnectJob::TimerIsRunning() const {
  return timer_.IsRunning();
}

void ConnectJob::LogConnectStart() {
  connect_timing_.connect_start = base::TimeTicks::Now();
  net_log().BeginEvent(net_log_connect_event_type_);
}

void ConnectJob::LogConnectCompletion(int net_error) {
  connect_timing_.connect_end = base::TimeTicks::Now();
  net_log().EndEventWithNetErrorCode(net_log_connect_event_type_, net_error);
}

void ConnectJob::OnTimeout() {
  // Make sure the socket is NULL before calling into |delegate|.
  SetSocket(nullptr);

  OnTimedOutInternal();

  net_log_.AddEvent(NetLogEventType::CONNECT_JOB_TIMED_OUT);

  NotifyDelegateOfCompletion(ERR_TIMED_OUT);
}

void ConnectJob::OnTimedOutInternal() {}

}  // namespace net

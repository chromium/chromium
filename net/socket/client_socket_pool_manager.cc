// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_manager.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/http/http_stream_factory.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/connect_job.h"
#include "net/ssl/ssl_config.h"

namespace net {

namespace {

// Limit of sockets of each socket pool.
int g_max_sockets_per_pool[] = {
  256,  // NORMAL_SOCKET_POOL
  256   // WEBSOCKET_SOCKET_POOL
};

static_assert(base::size(g_max_sockets_per_pool) ==
                  HttpNetworkSession::NUM_SOCKET_POOL_TYPES,
              "max sockets per pool length mismatch");

// Default to allow up to 6 connections per host. Experiment and tuning may
// try other values (greater than 0).  Too large may cause many problems, such
// as home routers blocking the connections!?!?  See http://crbug.com/12066.
//
// WebSocket connections are long-lived, and should be treated differently
// than normal other connections. Use a limit of 255, so the limit for wss will
// be the same as the limit for ws. Also note that Firefox uses a limit of 200.
// See http://crbug.com/486800
int g_max_sockets_per_group[] = {
    6,   // NORMAL_SOCKET_POOL
    255  // WEBSOCKET_SOCKET_POOL
};

static_assert(base::size(g_max_sockets_per_group) ==
                  HttpNetworkSession::NUM_SOCKET_POOL_TYPES,
              "max sockets per group length mismatch");

// The max number of sockets to allow per proxy server.  This applies both to
// http and SOCKS proxies.  See http://crbug.com/12066 and
// http://crbug.com/44501 for details about proxy server connection limits.
int g_max_sockets_per_proxy_server[] = {
  kDefaultMaxSocketsPerProxyServer,  // NORMAL_SOCKET_POOL
  kDefaultMaxSocketsPerProxyServer   // WEBSOCKET_SOCKET_POOL
};

static_assert(base::size(g_max_sockets_per_proxy_server) ==
                  HttpNetworkSession::NUM_SOCKET_POOL_TYPES,
              "max sockets per proxy server length mismatch");

ClientSocketPool::GroupId CreateGroupId(
    ClientSocketPoolManager::SocketGroupType group_type,
    const HostPortPair& endpoint,
    const ProxyInfo& proxy_info,
    PrivacyMode privacy_mode,
    const NetworkIsolationKey& network_isolation_key,
    bool disable_secure_dns) {
  // Build the string used to uniquely identify connections of this type.
  // Determine the host and port to connect to.
  DCHECK(!endpoint.IsEmpty());

  ClientSocketPool::SocketType socket_type =
      ClientSocketPool::SocketType::kHttp;
  if (group_type == ClientSocketPoolManager::SSL_GROUP)
    socket_type = ClientSocketPool::SocketType::kSsl;

  return ClientSocketPool::GroupId(endpoint, socket_type, privacy_mode,
                                   network_isolation_key, disable_secure_dns);
}

// TODO(https://crbug.com/921369) In order to resolve longstanding issues
// related to pooling distinguishable sockets together, get rid of SocketParams
// entirely.
scoped_refptr<ClientSocketPool::SocketParams> CreateSocketParams(
    const ClientSocketPool::GroupId& group_id,
    const ProxyServer& proxy_server,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy) {
  bool using_ssl = group_id.socket_type() == ClientSocketPool::SocketType::kSsl;
  bool using_proxy_ssl = proxy_server.is_http_like() && !proxy_server.is_http();
  return base::MakeRefCounted<ClientSocketPool::SocketParams>(
      using_ssl ? std::make_unique<SSLConfig>(ssl_config_for_origin) : nullptr,
      using_proxy_ssl ? std::make_unique<SSLConfig>(ssl_config_for_proxy)
                      : nullptr);
}

int InitSocketPoolHelper(
    ClientSocketPoolManager::SocketGroupType group_type,
    const HostPortPair& endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    bool is_for_websockets,
    PrivacyMode privacy_mode,
    const NetworkIsolationKey& network_isolation_key,
    bool disable_secure_dns,
    const SocketTag& socket_tag,
    const NetLogWithSource& net_log,
    int num_preconnect_streams,
    ClientSocketHandle* socket_handle,
    HttpNetworkSession::SocketPoolType socket_pool_type,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback) {
  bool using_ssl = group_type == ClientSocketPoolManager::SSL_GROUP;
  HostPortPair origin_host_port = endpoint;

  if (!using_ssl && session->params().testing_fixed_http_port != 0) {
    origin_host_port.set_port(session->params().testing_fixed_http_port);
  } else if (using_ssl && session->params().testing_fixed_https_port != 0) {
    origin_host_port.set_port(session->params().testing_fixed_https_port);
  }

  ClientSocketPool::GroupId connection_group =
      CreateGroupId(group_type, origin_host_port, proxy_info, privacy_mode,
                    network_isolation_key, disable_secure_dns);
  scoped_refptr<ClientSocketPool::SocketParams> socket_params =
      CreateSocketParams(connection_group, proxy_info.proxy_server(),
                         ssl_config_for_origin, ssl_config_for_proxy);

  ClientSocketPool* pool =
      session->GetSocketPool(socket_pool_type, proxy_info.proxy_server());
  ClientSocketPool::RespectLimits respect_limits =
      ClientSocketPool::RespectLimits::ENABLED;
  if ((request_load_flags & LOAD_IGNORE_LIMITS) != 0)
    respect_limits = ClientSocketPool::RespectLimits::DISABLED;

  base::Optional<NetworkTrafficAnnotationTag> proxy_annotation =
      proxy_info.is_direct() ? base::nullopt
                             : base::Optional<NetworkTrafficAnnotationTag>(
                                   proxy_info.traffic_annotation());
  if (num_preconnect_streams) {
    pool->RequestSockets(connection_group, std::move(socket_params),
                         proxy_annotation, num_preconnect_streams, net_log);
    return OK;
  }

  return socket_handle->Init(connection_group, std::move(socket_params),
                             proxy_annotation, request_priority, socket_tag,
                             respect_limits, std::move(callback),
                             proxy_auth_callback, pool, net_log);
}

}  // namespace

ClientSocketPoolManager::ClientSocketPoolManager() = default;
ClientSocketPoolManager::~ClientSocketPoolManager() = default;

// static
int ClientSocketPoolManager::max_sockets_per_pool(
    HttpNetworkSession::SocketPoolType pool_type) {
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  return g_max_sockets_per_pool[pool_type];
}

// static
void ClientSocketPoolManager::set_max_sockets_per_pool(
    HttpNetworkSession::SocketPoolType pool_type,
    int socket_count) {
  DCHECK_LT(0, socket_count);
  DCHECK_GT(1000, socket_count);  // Sanity check.
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  g_max_sockets_per_pool[pool_type] = socket_count;
  DCHECK_GE(g_max_sockets_per_pool[pool_type],
            g_max_sockets_per_group[pool_type]);
}

// static
int ClientSocketPoolManager::max_sockets_per_group(
    HttpNetworkSession::SocketPoolType pool_type) {
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  return g_max_sockets_per_group[pool_type];
}

// static
void ClientSocketPoolManager::set_max_sockets_per_group(
    HttpNetworkSession::SocketPoolType pool_type,
    int socket_count) {
  DCHECK_LT(0, socket_count);
  // The following is a sanity check... but we should NEVER be near this value.
  DCHECK_GT(100, socket_count);
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  g_max_sockets_per_group[pool_type] = socket_count;

  DCHECK_GE(g_max_sockets_per_pool[pool_type],
            g_max_sockets_per_group[pool_type]);
  DCHECK_GE(g_max_sockets_per_proxy_server[pool_type],
            g_max_sockets_per_group[pool_type]);
}

// static
int ClientSocketPoolManager::max_sockets_per_proxy_server(
    HttpNetworkSession::SocketPoolType pool_type) {
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  return g_max_sockets_per_proxy_server[pool_type];
}

// static
void ClientSocketPoolManager::set_max_sockets_per_proxy_server(
    HttpNetworkSession::SocketPoolType pool_type,
    int socket_count) {
  DCHECK_LT(0, socket_count);
  DCHECK_GT(100, socket_count);  // Sanity check.
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  // Assert this case early on. The max number of sockets per group cannot
  // exceed the max number of sockets per proxy server.
  DCHECK_LE(g_max_sockets_per_group[pool_type], socket_count);
  g_max_sockets_per_proxy_server[pool_type] = socket_count;
}

// static
base::TimeDelta ClientSocketPoolManager::unused_idle_socket_timeout(
    HttpNetworkSession::SocketPoolType pool_type) {
  return base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
      net::features::kNetUnusedIdleSocketTimeout,
      "unused_idle_socket_timeout_seconds",
#if defined(OS_ANDROID)
      60
#else
      10
#endif
      ));
}

int InitSocketHandleForHttpRequest(
    ClientSocketPoolManager::SocketGroupType group_type,
    const HostPortPair& endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    const NetworkIsolationKey& network_isolation_key,
    bool disable_secure_dns,
    const SocketTag& socket_tag,
    const NetLogWithSource& net_log,
    ClientSocketHandle* socket_handle,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback) {
  DCHECK(socket_handle);
  return InitSocketPoolHelper(
      group_type, endpoint, request_load_flags, request_priority, session,
      proxy_info, ssl_config_for_origin, ssl_config_for_proxy,
      false /* is_for_websockets */, privacy_mode, network_isolation_key,
      disable_secure_dns, socket_tag, net_log, 0, socket_handle,
      HttpNetworkSession::NORMAL_SOCKET_POOL, std::move(callback),
      proxy_auth_callback);
}

int InitSocketHandleForWebSocketRequest(
    ClientSocketPoolManager::SocketGroupType group_type,
    const HostPortPair& endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    const NetworkIsolationKey& network_isolation_key,
    const NetLogWithSource& net_log,
    ClientSocketHandle* socket_handle,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback) {
  DCHECK(socket_handle);

  // QUIC proxies are currently not supported through this method.
  DCHECK(!proxy_info.is_quic());

  return InitSocketPoolHelper(
      group_type, endpoint, request_load_flags, request_priority, session,
      proxy_info, ssl_config_for_origin, ssl_config_for_proxy,
      true /* is_for_websockets */, privacy_mode, network_isolation_key,
      false /*disable_secure_dns */, SocketTag(), net_log, 0, socket_handle,
      HttpNetworkSession::WEBSOCKET_SOCKET_POOL, std::move(callback),
      proxy_auth_callback);
}

int PreconnectSocketsForHttpRequest(
    ClientSocketPoolManager::SocketGroupType group_type,
    const HostPortPair& endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    PrivacyMode privacy_mode,
    const NetworkIsolationKey& network_isolation_key,
    bool disable_secure_dns,
    const NetLogWithSource& net_log,
    int num_preconnect_streams) {
  // QUIC proxies are currently not supported through this method.
  DCHECK(!proxy_info.is_quic());

  return InitSocketPoolHelper(
      group_type, endpoint, request_load_flags, request_priority, session,
      proxy_info, ssl_config_for_origin, ssl_config_for_proxy,
      false /* force_tunnel */, privacy_mode, network_isolation_key,
      disable_secure_dns, SocketTag(), net_log, num_preconnect_streams, nullptr,
      HttpNetworkSession::NORMAL_SOCKET_POOL, CompletionOnceCallback(),
      ClientSocketPool::ProxyAuthCallback());
}

}  // namespace net

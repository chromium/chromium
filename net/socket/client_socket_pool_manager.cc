// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_manager.h"

#include <array>
#include <memory>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "net/base/load_flags.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_stream_factory.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/connect_job.h"
#include "net/ssl/ssl_config.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {

namespace {

// The soft limit for active sockets per pool for this network process. This may
// be modified by set_socket_soft_cap_per_pool in tests, but should otherwise be
// as stated below.
auto g_socket_soft_cap_per_pool = std::to_array<size_t>({
    256,  // NORMAL_SOCKET_POOL
    256   // WEBSOCKET_SOCKET_POOL
});

static_assert(std::size(g_socket_soft_cap_per_pool) ==
                  HttpNetworkSession::NUM_SOCKET_POOL_TYPES,
              "socket soft cap per pool length mismatch");

// Default to allow up to 6 connections per host. Experiment and tuning may
// try other values (greater than 0).  Too large may cause many problems, such
// as home routers blocking the connections!?!?  See http://crbug.com/12066.
//
// WebSocket connections are long-lived, and should be treated differently
// than normal other connections. Use a limit of 255, so the limit for wss will
// be the same as the limit for ws. Also note that Firefox uses a limit of 200.
// See http://crbug.com/486800
auto g_max_sockets_per_group = std::to_array<size_t>({
    6,   // NORMAL_SOCKET_POOL
    255  // WEBSOCKET_SOCKET_POOL
});

static_assert(std::size(g_max_sockets_per_group) ==
                  HttpNetworkSession::NUM_SOCKET_POOL_TYPES,
              "max sockets per group length mismatch");

// The max number of sockets to allow per proxy chain.  This applies both to
// http and SOCKS proxies.  See http://crbug.com/12066 and
// http://crbug.com/44501 for details about proxy chain connection limits.
auto g_max_sockets_per_proxy_chain = std::to_array<size_t>({
    kDefaultMaxSocketsPerProxyChain,  // NORMAL_SOCKET_POOL
    kDefaultMaxSocketsPerProxyChain   // WEBSOCKET_SOCKET_POOL
});

static_assert(std::size(g_max_sockets_per_proxy_chain) ==
                  HttpNetworkSession::NUM_SOCKET_POOL_TYPES,
              "max sockets per proxy chain length mismatch");

// TODO(crbug.com/40609237) In order to resolve longstanding issues
// related to pooling distinguishable sockets together, get rid of SocketParams
// entirely.
scoped_refptr<ClientSocketPool::SocketParams> CreateSocketParams(
    const ClientSocketPool::GroupId& group_id,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs) {
  bool using_ssl = GURL::SchemeIsCryptographic(group_id.destination().scheme());
  return base::MakeRefCounted<ClientSocketPool::SocketParams>(
      using_ssl ? allowed_bad_certs : std::vector<SSLConfig::CertAndStatus>());
}

int InitSocketPoolHelper(
    url::SchemeHostPort endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    PrivacyMode privacy_mode,
    NetworkAnonymizationKey network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    const SocketTag& socket_tag,
    const NetLogWithSource& net_log,
    int num_preconnect_streams,
    ClientSocketHandle* socket_handle,
    HttpNetworkSession::SocketPoolType socket_pool_type,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback,
    bool fail_if_alias_requires_proxy_override) {
  DCHECK(endpoint.IsValid());

  session->ApplyTestingFixedPort(endpoint);

  bool disable_cert_network_fetches =
      !!(request_load_flags & LOAD_DISABLE_CERT_NETWORK_FETCHES);
  ClientSocketPool::GroupId connection_group(
      std::move(endpoint), privacy_mode, std::move(network_anonymization_key),
      secure_dns_policy, disable_cert_network_fetches);
  scoped_refptr<ClientSocketPool::SocketParams> socket_params =
      CreateSocketParams(connection_group, allowed_bad_certs);

  ClientSocketPool* pool =
      session->GetSocketPool(socket_pool_type, proxy_info.proxy_chain());
  ClientSocketPool::RespectLimits respect_limits =
      ClientSocketPool::RespectLimits::ENABLED;
  if ((request_load_flags & LOAD_IGNORE_LIMITS) != 0)
    respect_limits = ClientSocketPool::RespectLimits::DISABLED;

  std::optional<NetworkTrafficAnnotationTag> proxy_annotation =
      proxy_info.is_direct() ? std::nullopt
                             : std::optional<NetworkTrafficAnnotationTag>(
                                   proxy_info.traffic_annotation());
  if (num_preconnect_streams) {
    return pool->RequestSockets(connection_group, std::move(socket_params),
                                proxy_annotation, num_preconnect_streams,
                                fail_if_alias_requires_proxy_override,
                                std::move(callback), net_log);
  }

  return socket_handle->Init(
      connection_group, std::move(socket_params), proxy_annotation,
      request_priority, socket_tag, respect_limits, std::move(callback),
      proxy_auth_callback, fail_if_alias_requires_proxy_override, pool,
      net_log);
}

}  // namespace

ClientSocketPoolManager::ClientSocketPoolManager() = default;
ClientSocketPoolManager::~ClientSocketPoolManager() = default;

// static
size_t ClientSocketPoolManager::socket_soft_cap_per_pool(
    HttpNetworkSession::SocketPoolType pool_type) {
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  return g_socket_soft_cap_per_pool[pool_type];
}

// static
void ClientSocketPoolManager::set_socket_soft_cap_per_pool_for_test(
    HttpNetworkSession::SocketPoolType pool_type,
    size_t socket_count) {
  DCHECK_LT(0u, socket_count);     // At least one socket must be allowed.
  DCHECK_GE(2048u, socket_count);  // For now, we pick a ceiling of 2^11.
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  g_socket_soft_cap_per_pool[pool_type] = socket_count;
  DCHECK_GE(g_socket_soft_cap_per_pool[pool_type],
            g_max_sockets_per_group[pool_type]);
}

// static
size_t ClientSocketPoolManager::max_sockets_per_group(
    HttpNetworkSession::SocketPoolType pool_type) {
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  return g_max_sockets_per_group[pool_type];
}

// static
void ClientSocketPoolManager::set_max_sockets_per_group_for_test(
    HttpNetworkSession::SocketPoolType pool_type,
    size_t socket_count) {
  DCHECK_LT(0u, socket_count);    // At least one socket must be allowed.
  DCHECK_GE(512u, socket_count);  // For now, we pick a ceiling of 2^9.
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  g_max_sockets_per_group[pool_type] = socket_count;

  DCHECK_GE(g_socket_soft_cap_per_pool[pool_type],
            g_max_sockets_per_group[pool_type]);
  DCHECK_GE(g_max_sockets_per_proxy_chain[pool_type],
            g_max_sockets_per_group[pool_type]);
}

// static
size_t ClientSocketPoolManager::max_sockets_per_proxy_chain(
    HttpNetworkSession::SocketPoolType pool_type) {
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  return g_max_sockets_per_proxy_chain[pool_type];
}

// static
void ClientSocketPoolManager::set_max_sockets_per_proxy_chain(
    HttpNetworkSession::SocketPoolType pool_type,
    size_t socket_count) {
  DCHECK_LT(0u, socket_count);    // At least one socket must be allowed.
  DCHECK_GE(128u, socket_count);  // For now, we pick a ceiling of 2^7.
  DCHECK_LT(pool_type, HttpNetworkSession::NUM_SOCKET_POOL_TYPES);
  // Assert this case early on. The max number of sockets per group cannot
  // exceed the max number of sockets per proxy chain.
  DCHECK_LE(g_max_sockets_per_group[pool_type], socket_count);
  g_max_sockets_per_proxy_chain[pool_type] = socket_count;
}

// static
base::TimeDelta ClientSocketPoolManager::unused_idle_socket_timeout(
    HttpNetworkSession::SocketPoolType pool_type) {
  constexpr int kPreconnectIntervalSec = 60;
  return base::Seconds(kPreconnectIntervalSec);
}

int InitSocketHandleForHttpRequest(
    url::SchemeHostPort endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    PrivacyMode privacy_mode,
    NetworkAnonymizationKey network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    const SocketTag& socket_tag,
    const NetLogWithSource& net_log,
    ClientSocketHandle* socket_handle,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback,
    bool fail_if_alias_requires_proxy_override) {
  DCHECK(socket_handle);
  return InitSocketPoolHelper(
      std::move(endpoint), request_load_flags, request_priority, session,
      proxy_info, allowed_bad_certs, privacy_mode,
      std::move(network_anonymization_key), secure_dns_policy, socket_tag,
      net_log, 0, socket_handle, HttpNetworkSession::NORMAL_SOCKET_POOL,
      std::move(callback), proxy_auth_callback,
      fail_if_alias_requires_proxy_override);
}

int InitSocketHandleForWebSocketRequest(
    url::SchemeHostPort endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    PrivacyMode privacy_mode,
    NetworkAnonymizationKey network_anonymization_key,
    const NetLogWithSource& net_log,
    ClientSocketHandle* socket_handle,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback,
    bool fail_if_alias_requires_proxy_override) {
  DCHECK(socket_handle);

  // QUIC proxies are currently not supported through this method.
  DCHECK(proxy_info.is_direct() || !proxy_info.proxy_chain().Last().is_quic());

  // Expect websocket schemes (ws and wss) to be converted to the http(s)
  // equivalent.
  DCHECK(endpoint.scheme() == url::kHttpScheme ||
         endpoint.scheme() == url::kHttpsScheme);

  return InitSocketPoolHelper(
      std::move(endpoint), request_load_flags, request_priority, session,
      proxy_info, allowed_bad_certs, privacy_mode,
      std::move(network_anonymization_key), SecureDnsPolicy::kAllow,
      SocketTag(), net_log, 0, socket_handle,
      HttpNetworkSession::WEBSOCKET_SOCKET_POOL, std::move(callback),
      proxy_auth_callback, fail_if_alias_requires_proxy_override);
}

int PreconnectSocketsForHttpRequest(
    url::SchemeHostPort endpoint,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    PrivacyMode privacy_mode,
    NetworkAnonymizationKey network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    const NetLogWithSource& net_log,
    int num_preconnect_streams,
    bool fail_if_alias_requires_proxy_override,
    CompletionOnceCallback callback) {
  // Expect websocket schemes (ws and wss) to be converted to the http(s)
  // equivalent.
  DCHECK(endpoint.scheme() == url::kHttpScheme ||
         endpoint.scheme() == url::kHttpsScheme);

  return InitSocketPoolHelper(
      std::move(endpoint), request_load_flags, request_priority, session,
      proxy_info, allowed_bad_certs, privacy_mode,
      std::move(network_anonymization_key), secure_dns_policy, SocketTag(),
      net_log, num_preconnect_streams, nullptr,
      HttpNetworkSession::NORMAL_SOCKET_POOL, std::move(callback),
      ClientSocketPool::ProxyAuthCallback(),
      fail_if_alias_requires_proxy_override);
}

}  // namespace net

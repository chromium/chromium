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
#include "base/numerics/safe_conversions.h"
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

static constexpr size_t kSocketPoolTypesSize =
    std::to_underlying(HttpNetworkSession::SocketPoolType::kMaxValue) + 1;

// The soft limit for active sockets per pool for this network process. This may
// be modified by set_socket_soft_cap_per_pool in tests, but should otherwise be
// as stated below.
std::array<size_t, kSocketPoolTypesSize> g_socket_soft_cap_per_pool =
    std::to_array<size_t>({
        256,  // kNormal
        256   // kWebSocket
    });

// Default to allow up to 6 connections per host. Experiment and tuning may
// try other values (greater than 0).  Too large may cause many problems, such
// as home routers blocking the connections!?!?  See http://crbug.com/12066.
//
// WebSocket connections are long-lived, and should be treated differently
// than normal other connections. Use a limit of 255, so the limit for wss will
// be the same as the limit for ws. Also note that Firefox uses a limit of 200.
// See http://crbug.com/486800
std::array<size_t, kSocketPoolTypesSize> g_max_sockets_per_group =
    std::to_array<size_t>({
        6,   // kNormal
        255  // kWebSocket
    });

// Returns the limit for active connections through a specific proxy chain for
// this network process. `set_max_sockets_per_proxy_chain` can modify this.
std::array<size_t, kSocketPoolTypesSize>& GlobalMaxSocketPerProxyChain() {
  static std::array<size_t, kSocketPoolTypesSize>
      g_max_sockets_per_proxy_chain = []() {
        if (base::FeatureList::IsEnabled(features::kTcpSocketPoolProxyLimit)) {
          return std::to_array<size_t>({
              base::saturated_cast<size_t>(
                  features::kTcpSocketPoolProxyLimitNormal.Get()),  // kNormal
              base::saturated_cast<size_t>(
                  features::kTcpSocketPoolProxyLimitWebSocket
                      .Get())  // kWebSocket
          });
        }
        return std::to_array<size_t>({
            32,  // kNormal
            32   // kWebSocket
        });
      }();
  return g_max_sockets_per_proxy_chain;
}

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
    handles::NetworkHandle target_network,
    const NetLogWithSource& net_log,
    int num_preconnect_streams,
    ClientSocketHandle* socket_handle,
    HttpNetworkSession::SocketPoolType socket_pool_type,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback,
    ClientSocketPool::PreconnectCompletionCallback preconnect_callback) {
  DCHECK(endpoint.IsValid());

  session->ApplyTestingFixedPort(endpoint);

  bool disable_cert_network_fetches =
      !!(request_load_flags & LOAD_DISABLE_CERT_NETWORK_FETCHES);
  ClientSocketPool::GroupId connection_group(
      std::move(endpoint), privacy_mode, std::move(network_anonymization_key),
      secure_dns_policy, disable_cert_network_fetches, target_network);
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
                                std::move(preconnect_callback), net_log);
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
size_t ClientSocketPoolManager::socket_soft_cap_per_pool(
    HttpNetworkSession::SocketPoolType pool_type) {
  return g_socket_soft_cap_per_pool[std::to_underlying(pool_type)];
}

// static
void ClientSocketPoolManager::set_socket_soft_cap_per_pool_for_test(
    HttpNetworkSession::SocketPoolType pool_type,
    size_t socket_count) {
  DCHECK_LT(0u, socket_count);     // At least one socket must be allowed.
  DCHECK_GE(2048u, socket_count);  // For now, we pick a ceiling of 2^11.
  g_socket_soft_cap_per_pool[std::to_underlying(pool_type)] = socket_count;
  DCHECK_GE(g_socket_soft_cap_per_pool[std::to_underlying(pool_type)],
            g_max_sockets_per_group[std::to_underlying(pool_type)]);
}

// static
size_t ClientSocketPoolManager::max_sockets_per_group(
    HttpNetworkSession::SocketPoolType pool_type) {
  return g_max_sockets_per_group[std::to_underlying(pool_type)];
}

// static
void ClientSocketPoolManager::set_max_sockets_per_group_for_test(
    HttpNetworkSession::SocketPoolType pool_type,
    size_t socket_count) {
  DCHECK_LT(0u, socket_count);    // At least one socket must be allowed.
  DCHECK_GE(512u, socket_count);  // For now, we pick a ceiling of 2^9.
  g_max_sockets_per_group[std::to_underlying(pool_type)] = socket_count;

  DCHECK_GE(g_socket_soft_cap_per_pool[std::to_underlying(pool_type)],
            g_max_sockets_per_group[std::to_underlying(pool_type)]);
  DCHECK_GE(GlobalMaxSocketPerProxyChain()[std::to_underlying(pool_type)],
            g_max_sockets_per_group[std::to_underlying(pool_type)]);
}

// static
size_t ClientSocketPoolManager::max_sockets_per_proxy_chain(
    HttpNetworkSession::SocketPoolType pool_type) {
  return GlobalMaxSocketPerProxyChain()[std::to_underlying(pool_type)];
}

// static
void ClientSocketPoolManager::set_max_sockets_per_proxy_chain(
    HttpNetworkSession::SocketPoolType pool_type,
    size_t socket_count) {
  // LINT.IfChange(set_max_sockets_per_proxy_chain)
  // We set out explicit limits here because they are hard coded in the
  // enterprise policy MaxConnectionsPerProxy.
  CHECK_GE(socket_count, 6u);
  CHECK_LE(socket_count, 256u);
  // LINT.ThenChange(/net/socket/client_socket_pool_manager.cc:SetMaxConnectionsPerProxyChain)
  GlobalMaxSocketPerProxyChain()[std::to_underlying(pool_type)] = socket_count;
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
    handles::NetworkHandle target_network,
    const NetLogWithSource& net_log,
    ClientSocketHandle* socket_handle,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback) {
  DCHECK(socket_handle);
  return InitSocketPoolHelper(
      std::move(endpoint), request_load_flags, request_priority, session,
      proxy_info, allowed_bad_certs, privacy_mode,
      std::move(network_anonymization_key), secure_dns_policy, socket_tag,
      target_network, net_log, 0, socket_handle,
      HttpNetworkSession::SocketPoolType::kNormal, std::move(callback),
      proxy_auth_callback, ClientSocketPool::PreconnectCompletionCallback());
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
    handles::NetworkHandle target_network,
    const NetLogWithSource& net_log,
    ClientSocketHandle* socket_handle,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback) {
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
      SocketTag(), target_network, net_log, 0, socket_handle,
      HttpNetworkSession::SocketPoolType::kWebSocket, std::move(callback),
      proxy_auth_callback, ClientSocketPool::PreconnectCompletionCallback());
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
    handles::NetworkHandle target_network,
    const NetLogWithSource& net_log,
    int num_preconnect_streams,
    ClientSocketPool::PreconnectCompletionCallback callback) {
  // Expect websocket schemes (ws and wss) to be converted to the http(s)
  // equivalent.
  DCHECK(endpoint.scheme() == url::kHttpScheme ||
         endpoint.scheme() == url::kHttpsScheme);

  return InitSocketPoolHelper(
      std::move(endpoint), request_load_flags, request_priority, session,
      proxy_info, allowed_bad_certs, privacy_mode,
      std::move(network_anonymization_key), secure_dns_policy, SocketTag(),
      target_network, net_log, num_preconnect_streams, nullptr,
      HttpNetworkSession::SocketPoolType::kNormal, CompletionOnceCallback(),
      ClientSocketPool::ProxyAuthCallback(), std::move(callback));
}

}  // namespace net

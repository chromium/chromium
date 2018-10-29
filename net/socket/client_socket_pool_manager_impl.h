// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_IMPL_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <type_traits>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_export.h"
#include "net/cert/cert_database.h"
#include "net/http/http_network_session.h"
#include "net/socket/client_socket_pool_manager.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace net {

class CertVerifier;
class ChannelIDService;
class ClientSocketFactory;
class CTVerifier;
class HttpProxyClientSocketPool;
class HostResolver;
class NetLog;
class NetworkQualityEstimator;
class SocketPerformanceWatcherFactory;
class SOCKSClientSocketPool;
class SSLClientSocketPool;
class SSLConfigService;
class TransportClientSocketPool;
class TransportSecurityState;
class WebSocketEndpointLockManager;

class NET_EXPORT_PRIVATE ClientSocketPoolManagerImpl
    : public ClientSocketPoolManager,
      public CertDatabase::Observer {
 public:
  ClientSocketPoolManagerImpl(
      NetLog* net_log,
      ClientSocketFactory* socket_factory,
      SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
      NetworkQualityEstimator* network_quality_estimator,
      HostResolver* host_resolver,
      CertVerifier* cert_verifier,
      ChannelIDService* channel_id_service,
      TransportSecurityState* transport_security_state,
      CTVerifier* cert_transparency_verifier,
      CTPolicyEnforcer* ct_policy_enforcer,
      const std::string& ssl_session_cache_shard,
      SSLConfigService* ssl_config_service,
      WebSocketEndpointLockManager* websocket_endpoint_lock_manager,
      HttpNetworkSession::SocketPoolType pool_type);
  ~ClientSocketPoolManagerImpl() override;

  void FlushSocketPoolsWithError(int error) override;
  void CloseIdleSockets() override;

  TransportClientSocketPool* GetTransportSocketPool() override;

  SSLClientSocketPool* GetSSLSocketPool() override;

  SOCKSClientSocketPool* GetSocketPoolForSOCKSProxy(
      const HostPortPair& socks_proxy) override;

  HttpProxyClientSocketPool* GetSocketPoolForHTTPProxy(
      const HostPortPair& http_proxy) override;

  SSLClientSocketPool* GetSocketPoolForSSLWithProxy(
      const HostPortPair& proxy_server) override;

  // Creates a Value summary of the state of the socket pools.
  std::unique_ptr<base::Value> SocketPoolInfoToValue() const override;

  // CertDatabase::Observer methods:
  void OnCertDBChanged() override;

  void DumpMemoryStats(
      base::trace_event::ProcessMemoryDump* pmd,
      const std::string& parent_dump_absolute_name) const override;

 private:
  using TransportSocketPoolMap =
      std::map<HostPortPair, std::unique_ptr<TransportClientSocketPool>>;
  using SOCKSSocketPoolMap =
      std::map<HostPortPair, std::unique_ptr<SOCKSClientSocketPool>>;
  using HTTPProxySocketPoolMap =
      std::map<HostPortPair, std::unique_ptr<HttpProxyClientSocketPool>>;
  using SSLSocketPoolMap =
      std::map<HostPortPair, std::unique_ptr<SSLClientSocketPool>>;

  NetLog* const net_log_;
  ClientSocketFactory* const socket_factory_;
  SocketPerformanceWatcherFactory* socket_performance_watcher_factory_;
  NetworkQualityEstimator* network_quality_estimator_;
  HostResolver* const host_resolver_;
  CertVerifier* const cert_verifier_;
  ChannelIDService* const channel_id_service_;
  TransportSecurityState* const transport_security_state_;
  CTVerifier* const cert_transparency_verifier_;
  CTPolicyEnforcer* const ct_policy_enforcer_;
  const std::string ssl_session_cache_shard_;
  SSLConfigService* const ssl_config_service_;
  const HttpNetworkSession::SocketPoolType pool_type_;

  // Note: this ordering is important.

  std::unique_ptr<TransportClientSocketPool> transport_socket_pool_;
  std::unique_ptr<SSLClientSocketPool> ssl_socket_pool_;
  TransportSocketPoolMap transport_socket_pools_for_socks_proxies_;
  SOCKSSocketPoolMap socks_socket_pools_;
  TransportSocketPoolMap transport_socket_pools_for_http_proxies_;
  TransportSocketPoolMap transport_socket_pools_for_https_proxies_;
  SSLSocketPoolMap ssl_socket_pools_for_https_proxies_;
  HTTPProxySocketPoolMap http_proxy_socket_pools_;
  SSLSocketPoolMap ssl_socket_pools_for_proxies_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPoolManagerImpl);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_IMPL_H_

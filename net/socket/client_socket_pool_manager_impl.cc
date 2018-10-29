// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_manager_impl.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/socket/websocket_transport_client_socket_pool.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

class SocketPerformanceWatcherFactory;

namespace {

// Appends information about all |socket_pools| to the end of |list|.
template <class MapType>
void AddSocketPoolsToList(base::ListValue* list,
                          const MapType& socket_pools,
                          const std::string& type,
                          bool include_nested_pools) {
  for (auto it = socket_pools.begin(); it != socket_pools.end(); it++) {
    list->Append(it->second->GetInfoAsValue(it->first.ToString(),
                                            type,
                                            include_nested_pools));
  }
}

}  // namespace

ClientSocketPoolManagerImpl::ClientSocketPoolManagerImpl(
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
    HttpNetworkSession::SocketPoolType pool_type)
    : net_log_(net_log),
      socket_factory_(socket_factory),
      socket_performance_watcher_factory_(socket_performance_watcher_factory),
      network_quality_estimator_(network_quality_estimator),
      host_resolver_(host_resolver),
      cert_verifier_(cert_verifier),
      channel_id_service_(channel_id_service),
      transport_security_state_(transport_security_state),
      cert_transparency_verifier_(cert_transparency_verifier),
      ct_policy_enforcer_(ct_policy_enforcer),
      ssl_session_cache_shard_(ssl_session_cache_shard),
      ssl_config_service_(ssl_config_service),
      pool_type_(pool_type),
      transport_socket_pool_(pool_type ==
                                     HttpNetworkSession::WEBSOCKET_SOCKET_POOL
                                 ? new WebSocketTransportClientSocketPool(
                                       max_sockets_per_pool(pool_type),
                                       max_sockets_per_group(pool_type),
                                       host_resolver,
                                       socket_factory_,
                                       websocket_endpoint_lock_manager,
                                       net_log)
                                 : new TransportClientSocketPool(
                                       max_sockets_per_pool(pool_type),
                                       max_sockets_per_group(pool_type),
                                       host_resolver,
                                       socket_factory_,
                                       socket_performance_watcher_factory_,
                                       net_log)),
      ssl_socket_pool_(new SSLClientSocketPool(max_sockets_per_pool(pool_type),
                                               max_sockets_per_group(pool_type),
                                               cert_verifier,
                                               channel_id_service,
                                               transport_security_state,
                                               cert_transparency_verifier,
                                               ct_policy_enforcer,
                                               ssl_session_cache_shard,
                                               socket_factory,
                                               transport_socket_pool_.get(),
                                               nullptr /* no socks proxy */,
                                               nullptr /* no http proxy */,
                                               ssl_config_service,
                                               net_log)) {
  CertDatabase::GetInstance()->AddObserver(this);
}

ClientSocketPoolManagerImpl::~ClientSocketPoolManagerImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CertDatabase::GetInstance()->RemoveObserver(this);
}

void ClientSocketPoolManagerImpl::FlushSocketPoolsWithError(int error) {
  // Flush the highest level pools first, since higher level pools may release
  // stuff to the lower level pools.

  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_proxies_.begin();
       it != ssl_socket_pools_for_proxies_.end();
       ++it)
    it->second->FlushWithError(error);

  for (HTTPProxySocketPoolMap::const_iterator it =
       http_proxy_socket_pools_.begin();
       it != http_proxy_socket_pools_.end();
       ++it)
    it->second->FlushWithError(error);

  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_https_proxies_.begin();
       it != ssl_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->FlushWithError(error);

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_https_proxies_.begin();
       it != transport_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->FlushWithError(error);

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_http_proxies_.begin();
       it != transport_socket_pools_for_http_proxies_.end();
       ++it)
    it->second->FlushWithError(error);

  for (SOCKSSocketPoolMap::const_iterator it =
       socks_socket_pools_.begin();
       it != socks_socket_pools_.end();
       ++it)
    it->second->FlushWithError(error);

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_socks_proxies_.begin();
       it != transport_socket_pools_for_socks_proxies_.end();
       ++it)
    it->second->FlushWithError(error);

  ssl_socket_pool_->FlushWithError(error);
  transport_socket_pool_->FlushWithError(error);
}

void ClientSocketPoolManagerImpl::CloseIdleSockets() {
  // Close sockets in the highest level pools first, since higher level pools'
  // sockets may release stuff to the lower level pools.
  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_proxies_.begin();
       it != ssl_socket_pools_for_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (HTTPProxySocketPoolMap::const_iterator it =
       http_proxy_socket_pools_.begin();
       it != http_proxy_socket_pools_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_https_proxies_.begin();
       it != ssl_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_https_proxies_.begin();
       it != transport_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_http_proxies_.begin();
       it != transport_socket_pools_for_http_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (SOCKSSocketPoolMap::const_iterator it =
       socks_socket_pools_.begin();
       it != socks_socket_pools_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_socks_proxies_.begin();
       it != transport_socket_pools_for_socks_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  ssl_socket_pool_->CloseIdleSockets();
  transport_socket_pool_->CloseIdleSockets();
}

TransportClientSocketPool*
ClientSocketPoolManagerImpl::GetTransportSocketPool() {
  return transport_socket_pool_.get();
}

SSLClientSocketPool* ClientSocketPoolManagerImpl::GetSSLSocketPool() {
  return ssl_socket_pool_.get();
}

SOCKSClientSocketPool* ClientSocketPoolManagerImpl::GetSocketPoolForSOCKSProxy(
    const HostPortPair& socks_proxy) {
  SOCKSSocketPoolMap::const_iterator it = socks_socket_pools_.find(socks_proxy);
  if (it != socks_socket_pools_.end()) {
    DCHECK(base::ContainsKey(transport_socket_pools_for_socks_proxies_,
                             socks_proxy));
    return it->second.get();
  }

  DCHECK(!base::ContainsKey(transport_socket_pools_for_socks_proxies_,
                            socks_proxy));
  int sockets_per_proxy_server = max_sockets_per_proxy_server(pool_type_);
  int sockets_per_group = std::min(sockets_per_proxy_server,
                                   max_sockets_per_group(pool_type_));

  std::pair<TransportSocketPoolMap::iterator, bool> tcp_ret =
      transport_socket_pools_for_socks_proxies_.insert(std::make_pair(
          socks_proxy,
          std::make_unique<TransportClientSocketPool>(
              sockets_per_proxy_server, sockets_per_group, host_resolver_,
              socket_factory_, nullptr, net_log_)));
  DCHECK(tcp_ret.second);

  std::pair<SOCKSSocketPoolMap::iterator, bool> ret =
      socks_socket_pools_.insert(std::make_pair(
          socks_proxy,
          std::make_unique<SOCKSClientSocketPool>(
              sockets_per_proxy_server, sockets_per_group, host_resolver_,
              tcp_ret.first->second.get(), nullptr, net_log_)));

  return ret.first->second.get();
}

HttpProxyClientSocketPool*
ClientSocketPoolManagerImpl::GetSocketPoolForHTTPProxy(
    const HostPortPair& http_proxy) {
  HTTPProxySocketPoolMap::const_iterator it =
      http_proxy_socket_pools_.find(http_proxy);
  if (it != http_proxy_socket_pools_.end()) {
    DCHECK(base::ContainsKey(transport_socket_pools_for_http_proxies_,
                             http_proxy));
    DCHECK(base::ContainsKey(transport_socket_pools_for_https_proxies_,
                             http_proxy));
    DCHECK(base::ContainsKey(ssl_socket_pools_for_https_proxies_, http_proxy));
    return it->second.get();
  }

  DCHECK(
      !base::ContainsKey(transport_socket_pools_for_http_proxies_, http_proxy));
  DCHECK(!base::ContainsKey(transport_socket_pools_for_https_proxies_,
                            http_proxy));
  DCHECK(!base::ContainsKey(ssl_socket_pools_for_https_proxies_, http_proxy));

  int sockets_per_proxy_server = max_sockets_per_proxy_server(pool_type_);
  int sockets_per_group = std::min(sockets_per_proxy_server,
                                   max_sockets_per_group(pool_type_));

  std::pair<TransportSocketPoolMap::iterator, bool> tcp_http_ret =
      transport_socket_pools_for_http_proxies_.insert(std::make_pair(
          http_proxy,
          std::make_unique<TransportClientSocketPool>(
              sockets_per_proxy_server, sockets_per_group, host_resolver_,
              socket_factory_, socket_performance_watcher_factory_, net_log_)));
  DCHECK(tcp_http_ret.second);

  std::pair<TransportSocketPoolMap::iterator, bool> tcp_https_ret =
      transport_socket_pools_for_https_proxies_.insert(std::make_pair(
          http_proxy,
          std::make_unique<TransportClientSocketPool>(
              sockets_per_proxy_server, sockets_per_group, host_resolver_,
              socket_factory_, socket_performance_watcher_factory_, net_log_)));
  DCHECK(tcp_https_ret.second);

  std::pair<SSLSocketPoolMap::iterator, bool> ssl_https_ret =
      ssl_socket_pools_for_https_proxies_.insert(std::make_pair(
          http_proxy,
          std::make_unique<SSLClientSocketPool>(
              sockets_per_proxy_server, sockets_per_group, cert_verifier_,
              channel_id_service_, transport_security_state_,
              cert_transparency_verifier_, ct_policy_enforcer_,
              ssl_session_cache_shard_, socket_factory_,
              tcp_https_ret.first->second.get() /* https proxy */,
              nullptr /* no socks proxy */, nullptr /* no http proxy */,
              ssl_config_service_, net_log_)));
  DCHECK(tcp_https_ret.second);

  std::pair<HTTPProxySocketPoolMap::iterator, bool> ret =
      http_proxy_socket_pools_.insert(std::make_pair(
          http_proxy, std::make_unique<HttpProxyClientSocketPool>(
                          sockets_per_proxy_server, sockets_per_group,
                          tcp_http_ret.first->second.get(),
                          ssl_https_ret.first->second.get(),
                          network_quality_estimator_, net_log_)));

  return ret.first->second.get();
}

SSLClientSocketPool* ClientSocketPoolManagerImpl::GetSocketPoolForSSLWithProxy(
    const HostPortPair& proxy_server) {
  SSLSocketPoolMap::const_iterator it =
      ssl_socket_pools_for_proxies_.find(proxy_server);
  if (it != ssl_socket_pools_for_proxies_.end())
    return it->second.get();

  int sockets_per_proxy_server = max_sockets_per_proxy_server(pool_type_);
  int sockets_per_group = std::min(sockets_per_proxy_server,
                                   max_sockets_per_group(pool_type_));

  std::pair<SSLSocketPoolMap::iterator, bool> ret =
      ssl_socket_pools_for_proxies_.insert(std::make_pair(
          proxy_server,
          std::make_unique<SSLClientSocketPool>(
              sockets_per_proxy_server, sockets_per_group, cert_verifier_,
              channel_id_service_, transport_security_state_,
              cert_transparency_verifier_, ct_policy_enforcer_,
              ssl_session_cache_shard_, socket_factory_,
              nullptr, /* no tcp pool, we always go through a proxy */
              GetSocketPoolForSOCKSProxy(proxy_server),
              GetSocketPoolForHTTPProxy(proxy_server), ssl_config_service_,
              net_log_)));

  return ret.first->second.get();
}

std::unique_ptr<base::Value>
ClientSocketPoolManagerImpl::SocketPoolInfoToValue() const {
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  list->Append(transport_socket_pool_->GetInfoAsValue("transport_socket_pool",
                                                "transport_socket_pool",
                                                false));
  // Third parameter is false because |ssl_socket_pool_| uses
  // |transport_socket_pool_| internally, and do not want to add it a second
  // time.
  list->Append(ssl_socket_pool_->GetInfoAsValue("ssl_socket_pool",
                                                "ssl_socket_pool",
                                                false));
  AddSocketPoolsToList(list.get(), http_proxy_socket_pools_,
                       "http_proxy_socket_pool", true);
  AddSocketPoolsToList(list.get(), socks_socket_pools_, "socks_socket_pool",
                       true);

  // Third parameter is false because |ssl_socket_pools_for_proxies_| use
  // socket pools in |http_proxy_socket_pools_| and |socks_socket_pools_|.
  AddSocketPoolsToList(list.get(), ssl_socket_pools_for_proxies_,
                       "ssl_socket_pool_for_proxies", false);
  return std::move(list);
}

void ClientSocketPoolManagerImpl::OnCertDBChanged() {
  FlushSocketPoolsWithError(ERR_NETWORK_CHANGED);
}

void ClientSocketPoolManagerImpl::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_dump_absolute_name) const {
  return ssl_socket_pool_->DumpMemoryStats(pmd, parent_dump_absolute_name);
}

}  // namespace net

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_manager_impl.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/values.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/http/http_network_session.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/socket/transport_connect_job.h"
#include "net/socket/websocket_transport_client_socket_pool.h"

namespace net {

class SocketPerformanceWatcherFactory;

ClientSocketPoolManagerImpl::ClientSocketPoolManagerImpl(
    const CommonConnectJobParams& common_connect_job_params,
    const CommonConnectJobParams& websocket_common_connect_job_params,
    HttpNetworkSession::SocketPoolType pool_type,
    bool cleanup_on_ip_address_change)
    : common_connect_job_params_(common_connect_job_params),
      websocket_common_connect_job_params_(websocket_common_connect_job_params),
      pool_type_(pool_type),
      cleanup_on_ip_address_change_(cleanup_on_ip_address_change) {
  // |websocket_endpoint_lock_manager| must only be set for websocket
  // connections.
  DCHECK(!common_connect_job_params_.websocket_endpoint_lock_manager);
  DCHECK(websocket_common_connect_job_params.websocket_endpoint_lock_manager);
}

ClientSocketPoolManagerImpl::~ClientSocketPoolManagerImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ClientSocketPoolManagerImpl::FlushSocketPoolsWithError(
    int net_error,
    const char* net_log_reason_utf8) {
  for (const auto& it : socket_pools_) {
    it.second->FlushWithError(net_error, net_log_reason_utf8);
  }
}

void ClientSocketPoolManagerImpl::CloseIdleSockets(
    const char* net_log_reason_utf8) {
  for (const auto& it : socket_pools_) {
    it.second->CloseIdleSockets(net_log_reason_utf8);
  }
}

ClientSocketPool* ClientSocketPoolManagerImpl::GetSocketPool(
    const ProxyChain& proxy_chain) {
  SocketPoolMap::const_iterator it = socket_pools_.find(proxy_chain);
  if (it != socket_pools_.end())
    return it->second.get();

  int sockets_per_proxy_chain;
  int sockets_per_group;
  if (proxy_chain.is_direct()) {
    sockets_per_proxy_chain = max_sockets_per_pool(pool_type_);
    sockets_per_group = max_sockets_per_group(pool_type_);
  } else {
    sockets_per_proxy_chain = max_sockets_per_proxy_chain(pool_type_);
    sockets_per_group =
        std::min(sockets_per_proxy_chain, max_sockets_per_group(pool_type_));
  }

  std::unique_ptr<ClientSocketPool> new_pool;

  // Use specialized WebSockets pool for WebSockets when no proxies are in use.
  if (pool_type_ == HttpNetworkSession::WEBSOCKET_SOCKET_POOL &&
      proxy_chain.is_direct()) {
    new_pool = std::make_unique<WebSocketTransportClientSocketPool>(
        sockets_per_proxy_chain, sockets_per_group, proxy_chain,
        &websocket_common_connect_job_params_);
  } else {
    new_pool = std::make_unique<TransportClientSocketPool>(
        sockets_per_proxy_chain, sockets_per_group,
        unused_idle_socket_timeout(pool_type_), proxy_chain,
        pool_type_ == HttpNetworkSession::WEBSOCKET_SOCKET_POOL,
        &common_connect_job_params_, cleanup_on_ip_address_change_);
  }

  std::pair<SocketPoolMap::iterator, bool> ret =
      socket_pools_.emplace(proxy_chain, std::move(new_pool));
  return ret.first->second.get();
}

base::Value ClientSocketPoolManagerImpl::SocketPoolInfoToValue() const {
  base::Value::List list;
  for (const auto& socket_pool : socket_pools_) {
    // TODO(menke): Is this really needed?
    const char* type;
    // Note that it's actually the last proxy that determines the type of socket
    // pool, although for SOCKS proxy chains, multi-proxy chains aren't
    // supported.
    const ProxyChain& proxy_chain = socket_pool.first;
    if (proxy_chain.is_direct()) {
      type = "transport_socket_pool";
    } else if (proxy_chain.Last().is_socks()) {
      type = "socks_socket_pool";
    } else {
      type = "http_proxy_socket_pool";
    }
    list.Append(
        socket_pool.second->GetInfoAsValue(proxy_chain.ToDebugString(), type));
  }

  return base::Value(std::move(list));
}

}  // namespace net

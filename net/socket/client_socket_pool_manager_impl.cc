// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_manager_impl.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "net/base/proxy_server.h"
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
    HttpNetworkSession::SocketPoolType pool_type)
    : common_connect_job_params_(common_connect_job_params),
      websocket_common_connect_job_params_(websocket_common_connect_job_params),
      pool_type_(pool_type) {
  // |websocket_endpoint_lock_manager| must only be set for websocket
  // connections.
  DCHECK(!common_connect_job_params_.websocket_endpoint_lock_manager);
  DCHECK(websocket_common_connect_job_params.websocket_endpoint_lock_manager);
}

ClientSocketPoolManagerImpl::~ClientSocketPoolManagerImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ClientSocketPoolManagerImpl::FlushSocketPoolsWithError(int error) {
  for (const auto& it : socket_pools_) {
    it.second->FlushWithError(error);
  }
}

void ClientSocketPoolManagerImpl::CloseIdleSockets() {
  for (const auto& it : socket_pools_) {
    it.second->CloseIdleSockets();
  }
}

ClientSocketPool* ClientSocketPoolManagerImpl::GetSocketPool(
    const ProxyServer& proxy_server) {
  SocketPoolMap::const_iterator it = socket_pools_.find(proxy_server);
  if (it != socket_pools_.end())
    return it->second.get();

  int sockets_per_proxy_server;
  int sockets_per_group;
  if (proxy_server.is_direct()) {
    sockets_per_proxy_server = max_sockets_per_pool(pool_type_);
    sockets_per_group = max_sockets_per_group(pool_type_);
  } else {
    sockets_per_proxy_server = max_sockets_per_proxy_server(pool_type_);
    sockets_per_group =
        std::min(sockets_per_proxy_server, max_sockets_per_group(pool_type_));
  }

  std::unique_ptr<ClientSocketPool> new_pool;

  // Use specialized WebSockets pool for WebSockets when no proxy is in use.
  if (pool_type_ == HttpNetworkSession::WEBSOCKET_SOCKET_POOL &&
      proxy_server.is_direct()) {
    new_pool = std::make_unique<WebSocketTransportClientSocketPool>(
        sockets_per_proxy_server, sockets_per_group, proxy_server,
        &websocket_common_connect_job_params_);
  } else {
    new_pool = std::make_unique<TransportClientSocketPool>(
        sockets_per_proxy_server, sockets_per_group,
        unused_idle_socket_timeout(pool_type_), proxy_server,
        pool_type_ == HttpNetworkSession::WEBSOCKET_SOCKET_POOL,
        &common_connect_job_params_);
  }

  std::pair<SocketPoolMap::iterator, bool> ret =
      socket_pools_.insert(std::make_pair(proxy_server, std::move(new_pool)));
  return ret.first->second.get();
}

std::unique_ptr<base::Value>
ClientSocketPoolManagerImpl::SocketPoolInfoToValue() const {
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  for (const auto& socket_pool : socket_pools_) {
    // TODO(menke): Is this really needed?
    const char* type;
    if (socket_pool.first.is_direct()) {
      type = "transport_socket_pool";
    } else if (socket_pool.first.is_socks()) {
      type = "socks_socket_pool";
    } else {
      type = "http_proxy_socket_pool";
    }
    list->Append(
        socket_pool.second->GetInfoAsValue(socket_pool.first.ToURI(), type));
  }

  return std::move(list);
}

void ClientSocketPoolManagerImpl::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_dump_absolute_name) const {
  SocketPoolMap::const_iterator socket_pool =
      socket_pools_.find(ProxyServer::Direct());
  if (socket_pool == socket_pools_.end())
    return;
  socket_pool->second->DumpMemoryStats(pmd, parent_dump_absolute_name);
}

}  // namespace net

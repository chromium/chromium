// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_IMPL_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <type_traits>

#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/http/http_network_session.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/connect_job.h"

namespace net {

class ProxyChain;
class ClientSocketPool;

class NET_EXPORT_PRIVATE ClientSocketPoolManagerImpl
    : public ClientSocketPoolManager {
 public:
  // `websocket_common_connect_job_params` is only used for direct WebSocket
  // connections (No proxies in use). It's never used if `pool_type` is not
  // HttpNetworkSession::SocketPoolType::WEBSOCKET_SOCKET_POOL.
  ClientSocketPoolManagerImpl(
      const CommonConnectJobParams& common_connect_job_params,
      const CommonConnectJobParams& websocket_common_connect_job_params,
      HttpNetworkSession::SocketPoolType pool_type,
      bool cleanup_on_ip_address_change = true);

  ClientSocketPoolManagerImpl(const ClientSocketPoolManagerImpl&) = delete;
  ClientSocketPoolManagerImpl& operator=(const ClientSocketPoolManagerImpl&) =
      delete;

  ~ClientSocketPoolManagerImpl() override;

  void FlushSocketPoolsWithError(int net_error,
                                 const char* net_log_reason_utf8) override;
  void CloseIdleSockets(const char* net_log_reason_utf8) override;

  ClientSocketPool* GetSocketPool(const ProxyChain& proxy_chain) override;

  // Creates a Value summary of the state of the socket pools.
  base::Value SocketPoolInfoToValue() const override;

 private:
  using SocketPoolMap = std::map<ProxyChain, std::unique_ptr<ClientSocketPool>>;

  const CommonConnectJobParams common_connect_job_params_;
  // Used only for direct WebSocket connections (i.e., no proxy in use).
  const CommonConnectJobParams websocket_common_connect_job_params_;

  const HttpNetworkSession::SocketPoolType pool_type_;

  const bool cleanup_on_ip_address_change_;

  SocketPoolMap socket_pools_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_IMPL_H_

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
#include "net/http/http_network_session.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/connect_job.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace net {

class ProxyServer;
class ClientSocketPool;

class NET_EXPORT_PRIVATE ClientSocketPoolManagerImpl
    : public ClientSocketPoolManager {
 public:
  // |websocket_common_connect_job_params| is only used for direct WebSocket
  // connections (No proxy in use). It's never used if |pool_type| is not
  // HttpNetworkSession::SocketPoolType::WEBSOCKET_SOCKET_POOL.
  ClientSocketPoolManagerImpl(
      const CommonConnectJobParams& common_connect_job_params,
      const CommonConnectJobParams& websocket_common_connect_job_params,
      HttpNetworkSession::SocketPoolType pool_type);
  ~ClientSocketPoolManagerImpl() override;

  void FlushSocketPoolsWithError(int error) override;
  void CloseIdleSockets() override;

  ClientSocketPool* GetSocketPool(const ProxyServer& proxy_server) override;

  // Creates a Value summary of the state of the socket pools.
  std::unique_ptr<base::Value> SocketPoolInfoToValue() const override;

  void DumpMemoryStats(
      base::trace_event::ProcessMemoryDump* pmd,
      const std::string& parent_dump_absolute_name) const override;

 private:
  using SocketPoolMap =
      std::map<ProxyServer, std::unique_ptr<ClientSocketPool>>;

  const CommonConnectJobParams common_connect_job_params_;
  // Used only for direct WebSocket connections (i.e., no proxy in use).
  const CommonConnectJobParams websocket_common_connect_job_params_;

  const HttpNetworkSession::SocketPoolType pool_type_;

  SocketPoolMap socket_pools_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPoolManagerImpl);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_MANAGER_IMPL_H_

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/mock_client_socket_pool_manager.h"

#include <utility>

#include "base/values.h"
#include "net/socket/client_socket_pool.h"

namespace net {

MockClientSocketPoolManager::MockClientSocketPoolManager() = default;
MockClientSocketPoolManager::~MockClientSocketPoolManager() = default;

void MockClientSocketPoolManager::SetSocketPool(
    const ProxyChain& proxy_chain,
    std::unique_ptr<ClientSocketPool> pool) {
  socket_pools_[proxy_chain] = std::move(pool);
}

void MockClientSocketPoolManager::FlushSocketPoolsWithError(
    int error,
    const char* net_log_reason_utf8) {
  NOTIMPLEMENTED();
}

void MockClientSocketPoolManager::CloseIdleSockets(
    const char* net_log_reason_utf8) {
  NOTIMPLEMENTED();
}

ClientSocketPool* MockClientSocketPoolManager::GetSocketPool(
    const ProxyChain& proxy_chain) {
  ClientSocketPoolMap::const_iterator it = socket_pools_.find(proxy_chain);
  if (it != socket_pools_.end())
    return it->second.get();
  return nullptr;
}

base::Value MockClientSocketPoolManager::SocketPoolInfoToValue() const {
  NOTIMPLEMENTED();
  return base::Value();
}

}  // namespace net

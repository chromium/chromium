// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
    const ProxyServer& proxy_server,
    std::unique_ptr<ClientSocketPool> pool) {
  socket_pools_[proxy_server] = std::move(pool);
}

void MockClientSocketPoolManager::FlushSocketPoolsWithError(int error) {
  NOTIMPLEMENTED();
}

void MockClientSocketPoolManager::CloseIdleSockets() {
  NOTIMPLEMENTED();
}

ClientSocketPool* MockClientSocketPoolManager::GetSocketPool(
    const ProxyServer& proxy_server) {
  ClientSocketPoolMap::const_iterator it = socket_pools_.find(proxy_server);
  if (it != socket_pools_.end())
    return it->second.get();
  return nullptr;
}

std::unique_ptr<base::Value>
MockClientSocketPoolManager::SocketPoolInfoToValue() const {
  NOTIMPLEMENTED();
  return std::unique_ptr<base::Value>(nullptr);
}

void MockClientSocketPoolManager::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_dump_absolute_name) const {}

}  // namespace net

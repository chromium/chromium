// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/server/http_connection.h"

#include <utility>

#include "base/check.h"
#include "net/socket/stream_socket.h"
#include "services/network/public/cpp/server/web_socket.h"

namespace network {

namespace server {

HttpConnection::HttpConnection(
    int id,
    mojo::PendingRemote<mojom::TCPConnectedSocket> socket,
    mojo::ScopedDataPipeConsumerHandle socket_receive_handle,
    mojo::ScopedDataPipeProducerHandle socket_send_handle,
    const net::IPEndPoint& peer_addr)
    : id_(id),
      socket_(std::move(socket)),
      socket_receive_handle_(std::move(socket_receive_handle)),
      receive_pipe_watcher_(FROM_HERE,
                            mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC),
      socket_send_handle_(std::move(socket_send_handle)),
      send_pipe_watcher_(FROM_HERE,
                         mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC),
      peer_addr_(peer_addr) {}

HttpConnection::~HttpConnection() = default;

void HttpConnection::SetWebSocket(std::unique_ptr<WebSocket> web_socket) {
  DCHECK(!web_socket_);
  web_socket_ = std::move(web_socket);
}

}  // namespace server

}  // namespace network

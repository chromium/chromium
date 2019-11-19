// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/p2p/socket_tcp_server.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"
#include "services/network/p2p/socket_manager.h"
#include "services/network/p2p/socket_tcp.h"
#include "services/network/public/cpp/p2p_param_traits.h"

namespace {
const int kListenBacklog = 5;
}  // namespace

namespace network {

P2PSocketTcpServer::P2PSocketTcpServer(
    Delegate* delegate,
    mojo::PendingRemote<mojom::P2PSocketClient> client,
    mojo::PendingReceiver<mojom::P2PSocket> socket,
    P2PSocketType client_type)
    : P2PSocket(delegate, std::move(client), std::move(socket), P2PSocket::TCP),
      client_type_(client_type),
      socket_(new net::TCPServerSocket(nullptr, net::NetLogSource())),
      accept_callback_(base::BindRepeating(&P2PSocketTcpServer::OnAccepted,
                                           base::Unretained(this))) {}

P2PSocketTcpServer::~P2PSocketTcpServer() = default;

// TODO(guidou): Add support for port range.
void P2PSocketTcpServer::Init(const net::IPEndPoint& local_address,
                              uint16_t min_port,
                              uint16_t max_port,
                              const P2PHostAndIPEndPoint& remote_address) {
  int result = socket_->Listen(local_address, kListenBacklog);
  if (result < 0) {
    LOG(ERROR) << "Listen() failed: " << result;
    OnError();
    return;
  }

  result = socket_->GetLocalAddress(&local_address_);
  if (result < 0) {
    LOG(ERROR) << "P2PSocketTcpServer::Init(): can't to get local address: "
               << result;
    OnError();
    return;
  }
  VLOG(1) << "Local address: " << local_address_.ToString();

  // NOTE: Remote address can be empty as socket is just listening
  // in this state.
  client_->SocketCreated(local_address_, remote_address.ip_address);
  DoAccept();
}

void P2PSocketTcpServer::DoAccept() {
  while (true) {
    int result = socket_->Accept(&accept_socket_, accept_callback_);
    if (result == net::ERR_IO_PENDING) {
      break;
    } else {
      HandleAcceptResult(result);
    }
  }
}

void P2PSocketTcpServer::HandleAcceptResult(int result) {
  if (result < 0) {
    if (result != net::ERR_IO_PENDING)
      OnError();
    return;
  }

  net::IPEndPoint remote_address;
  if (accept_socket_->GetPeerAddress(&remote_address) != net::OK) {
    LOG(ERROR) << "Failed to get address of an accepted socket.";
    accept_socket_.reset();
    return;
  }

  mojo::PendingRemote<mojom::P2PSocket> socket_pending_remote;
  auto socket_receiver = socket_pending_remote.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<mojom::P2PSocketClient> client;
  client_->IncomingTcpConnection(remote_address,
                                 std::move(socket_pending_remote),
                                 client.InitWithNewPipeAndPassReceiver());

  std::unique_ptr<P2PSocketTcpBase> p2p_socket;
  if (client_type_ == P2P_SOCKET_TCP_CLIENT) {
    p2p_socket = std::make_unique<P2PSocketTcp>(delegate_, std::move(client),
                                                std::move(socket_receiver),
                                                client_type_, nullptr);
  } else {
    p2p_socket = std::make_unique<P2PSocketStunTcp>(
        delegate_, std::move(client), std::move(socket_receiver), client_type_,
        nullptr);
  }

  P2PSocketTcpBase* p2p_socket_ptr = p2p_socket.get();
  delegate_->AddAcceptedConnection(std::move(p2p_socket));

  // InitAccepted() may call delegate_->DestroySocket(), so it must be
  // called after AddAcceptedConnection().
  p2p_socket_ptr->InitAccepted(remote_address, std::move(accept_socket_));
}

void P2PSocketTcpServer::OnAccepted(int result) {
  HandleAcceptResult(result);
  if (result == net::OK)
    DoAccept();
}

void P2PSocketTcpServer::Send(
    const std::vector<int8_t>& data,
    const P2PPacketInfo& packet_info,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  OnError();
}

void P2PSocketTcpServer::SetOption(P2PSocketOption option, int32_t value) {
  // Currently we don't have use case tcp server sockets are used for p2p.
}

}  // namespace network

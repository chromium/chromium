// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TCP_BOUND_SOCKET_H_
#define SERVICES_NETWORK_TCP_BOUND_SOCKET_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/tcp_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/tcp_server_socket.h"

namespace net {
class IPEndPoint;
class NetLog;
}  // namespace net

namespace network {
class SocketFactory;

// A socket bound to an address. Can be converted into either a TCPServerSocket
// or a TCPConnectedSocket.
class COMPONENT_EXPORT(NETWORK_SERVICE) TCPBoundSocket
    : public mojom::TCPBoundSocket {
 public:
  // Constructs a TCPBoundSocket. |socket_factory| must outlive |this|. When a
  // new connection is accepted, |socket_factory| will be notified to take
  // ownership of the connection.
  TCPBoundSocket(SocketFactory* socket_factory,
                 net::NetLog* net_log,
                 const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~TCPBoundSocket() override;

  // Attempts to bind a socket to the specified address. Returns net::OK on
  // success, setting |local_addr_out| to the bound address. Returns a network
  // error code on failure. Must be called before Listen() or Connect().
  int Bind(const net::IPEndPoint& local_addr, net::IPEndPoint* local_addr_out);

  // Sets the id used to remove the socket from the owning ReceiverSet. Must be
  // called before Listen() or Connect().
  void set_id(mojo::ReceiverId receiver_id) { receiver_id_ = receiver_id; }

  // mojom::TCPBoundSocket implementation.
  void Listen(uint32_t backlog,
              mojo::PendingReceiver<mojom::TCPServerSocket> receiver,
              ListenCallback callback) override;
  void Connect(const net::AddressList& remote_addr,
               mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
               mojo::PendingReceiver<mojom::TCPConnectedSocket> receiver,
               mojo::PendingRemote<mojom::SocketObserver> observer,
               ConnectCallback callback) override;

 private:
  void OnConnectComplete(int result,
                         const base::Optional<net::IPEndPoint>& local_addr,
                         const base::Optional<net::IPEndPoint>& peer_addr,
                         mojo::ScopedDataPipeConsumerHandle receive_stream,
                         mojo::ScopedDataPipeProducerHandle send_stream);

  virtual int ListenInternal(int backlog);

  net::IPEndPoint bind_address_;

  mojo::ReceiverId receiver_id_ = -1;
  SocketFactory* const socket_factory_;
  std::unique_ptr<net::TCPSocket> socket_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  mojo::PendingReceiver<mojom::TCPConnectedSocket> connected_socket_receiver_;
  ConnectCallback connect_callback_;

  // Takes ownership of |socket_| if Connect() is called.
  std::unique_ptr<TCPConnectedSocket> connecting_socket_;

  base::WeakPtrFactory<TCPBoundSocket> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TCPBoundSocket);
};

}  // namespace network

#endif  // SERVICES_NETWORK_TCP_BOUND_SOCKET_H_

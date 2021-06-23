// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SOCKET_FACTORY_H_
#define SERVICES_NETWORK_SOCKET_FACTORY_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/tcp_bound_socket.h"
#include "services/network/tcp_connected_socket.h"
#include "services/network/tcp_server_socket.h"
#include "services/network/tls_socket_factory.h"

namespace net {
class ClientSocketFactory;
class NetLog;
}  // namespace net

namespace network {

// Helper class that handles socket requests. It takes care of destroying
// socket implementation instances when mojo  pipes are broken.
class COMPONENT_EXPORT(NETWORK_SERVICE) SocketFactory
    : public TCPServerSocket::Delegate {
 public:
  // Constructs a SocketFactory. If |net_log| is non-null, it is used to
  // log NetLog events when logging is enabled. |net_log| used to must outlive
  // |this|.
  SocketFactory(net::NetLog* net_log,
                net::URLRequestContext* url_request_context);
  virtual ~SocketFactory();

  // These all correspond to the NetworkContext methods of the same name.
  void CreateUDPSocket(mojo::PendingReceiver<mojom::UDPSocket> receiver,
                       mojo::PendingRemote<mojom::UDPSocketListener> listener);
  void CreateTCPServerSocket(
      const net::IPEndPoint& local_addr,
      int backlog,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<mojom::TCPServerSocket> receiver,
      mojom::NetworkContext::CreateTCPServerSocketCallback callback);
  void CreateTCPConnectedSocket(
      const base::Optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<mojom::TCPConnectedSocket> receiver,
      mojo::PendingRemote<mojom::SocketObserver> observer,
      mojom::NetworkContext::CreateTCPConnectedSocketCallback callback);
  void CreateTCPBoundSocket(
      const net::IPEndPoint& local_addr,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<mojom::TCPBoundSocket> receiver,
      mojom::NetworkContext::CreateTCPBoundSocketCallback callback);

  // Destroys the specified BoundSocket object.
  void DestroyBoundSocket(mojo::ReceiverId bound_socket_id);

  // Invoked when a BoundSocket successfully starts listening. Destroys the
  // BoundSocket object, adding a binding for the provided TCPServerSocket in
  // its place.
  void OnBoundSocketListening(
      mojo::ReceiverId bound_socket_id,
      std::unique_ptr<TCPServerSocket> server_socket,
      mojo::PendingReceiver<mojom::TCPServerSocket> server_socket_receiver);

  // Invoked when a BoundSocket successfully establishes a connection. Destroys
  // the BoundSocket object, adding a binding for the provided
  // TCPConnectedSocket in its place.
  void OnBoundSocketConnected(
      mojo::ReceiverId bound_socket_id,
      std::unique_ptr<TCPConnectedSocket> connected_socket,
      mojo::PendingReceiver<mojom::TCPConnectedSocket>
          connected_socket_receiver);

  TLSSocketFactory* tls_socket_factory() { return &tls_socket_factory_; }

 private:
  // TCPServerSocket::Delegate implementation:
  void OnAccept(
      std::unique_ptr<TCPConnectedSocket> socket,
      mojo::PendingReceiver<mojom::TCPConnectedSocket> receiver) override;

  net::NetLog* const net_log_;

  net::ClientSocketFactory* client_socket_factory_;
  TLSSocketFactory tls_socket_factory_;
  mojo::UniqueReceiverSet<mojom::UDPSocket> udp_socket_receivers_;
  mojo::UniqueReceiverSet<mojom::TCPServerSocket> tcp_server_socket_receivers_;
  mojo::UniqueReceiverSet<mojom::TCPConnectedSocket>
      tcp_connected_socket_receiver_;
  mojo::UniqueReceiverSet<mojom::TCPBoundSocket> tcp_bound_socket_receivers_;

  DISALLOW_COPY_AND_ASSIGN(SocketFactory);
};

}  // namespace network

#endif  // SERVICES_NETWORK_SOCKET_FACTORY_H_

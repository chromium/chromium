// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TCP_CONNECTED_SOCKET_H_
#define SERVICES_NETWORK_TCP_CONNECTED_SOCKET_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/mojom/address_family.mojom.h"
#include "services/network/public/mojom/ip_endpoint.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/socket_data_pump.h"
#include "services/network/tls_socket_factory.h"

namespace net {
class NetLog;
class ClientSocketFactory;
class TransportClientSocket;
}  // namespace net

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) TCPConnectedSocket
    : public mojom::TCPConnectedSocket,
      public SocketDataPump::Delegate,
      public TLSSocketFactory::Delegate {
 public:
  // Max send/receive buffer size the consumer is allowed to set. Exposed for
  // testing.
  static const int kMaxBufferSize;

  // If |client_socket_factory| is nullptr, consumers must use
  // ConnectWithSocket() instead of Connect().
  TCPConnectedSocket(
      mojo::PendingRemote<mojom::SocketObserver> observer,
      net::NetLog* net_log,
      TLSSocketFactory* tls_socket_factory,
      net::ClientSocketFactory* client_socket_factory,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  TCPConnectedSocket(
      mojo::PendingRemote<mojom::SocketObserver> observer,
      std::unique_ptr<net::TransportClientSocket> socket,
      mojo::ScopedDataPipeProducerHandle receive_pipe_handle,
      mojo::ScopedDataPipeConsumerHandle send_pipe_handle,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~TCPConnectedSocket() override;

  void Connect(
      const base::Optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      mojom::NetworkContext::CreateTCPConnectedSocketCallback callback);

  // Tries to connects using the provided TCPClientSocket. |socket| owns the
  // list of addresses to try to connect to, so this method doesn't need any
  // addresses as input.
  void ConnectWithSocket(
      std::unique_ptr<net::TransportClientSocket> socket,
      mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      mojom::NetworkContext::CreateTCPConnectedSocketCallback callback);

  // mojom::TCPConnectedSocket implementation.
  void UpgradeToTLS(
      const net::HostPortPair& host_port_pair,
      mojom::TLSClientSocketOptionsPtr socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<mojom::TLSClientSocket> receiver,
      mojo::PendingRemote<mojom::SocketObserver> observer,
      mojom::TCPConnectedSocket::UpgradeToTLSCallback callback) override;
  void SetSendBufferSize(int send_buffer_size,
                         SetSendBufferSizeCallback callback) override;
  void SetReceiveBufferSize(int send_buffer_size,
                            SetSendBufferSizeCallback callback) override;
  void SetNoDelay(bool no_delay, SetNoDelayCallback callback) override;
  void SetKeepAlive(bool enable,
                    int32_t delay_secs,
                    SetKeepAliveCallback callback) override;

 private:
  // Invoked when net::TCPClientSocket::Connect() completes.
  void OnConnectCompleted(int net_result);

  // SocketDataPump::Delegate implementation.
  void OnNetworkReadError(int net_error) override;
  void OnNetworkWriteError(int net_error) override;
  void OnShutdown() override;

  // TLSSocketFactory::Delegate implementation.
  const net::StreamSocket* BorrowSocket() override;
  std::unique_ptr<net::StreamSocket> TakeSocket() override;

  const mojo::Remote<mojom::SocketObserver> observer_;

  net::NetLog* const net_log_;
  net::ClientSocketFactory* const client_socket_factory_;
  TLSSocketFactory* tls_socket_factory_;

  std::unique_ptr<net::TransportClientSocket> socket_;

  mojom::NetworkContext::CreateTCPConnectedSocketCallback connect_callback_;

  base::OnceClosure pending_upgrade_to_tls_callback_;

  std::unique_ptr<SocketDataPump> socket_data_pump_;

  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  DISALLOW_COPY_AND_ASSIGN(TCPConnectedSocket);
};

}  // namespace network

#endif  // SERVICES_NETWORK_TCP_CONNECTED_SOCKET_H_

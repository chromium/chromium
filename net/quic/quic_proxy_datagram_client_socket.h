// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_PROXY_DATAGRAM_CLIENT_SOCKET_H_
#define NET_QUIC_QUIC_PROXY_DATAGRAM_CLIENT_SOCKET_H_

#include <stdint.h>

#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_chromium_client_stream.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/udp_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/scheme_host_port.h"

namespace net {

// A client socket that uses a QUIC proxy as the transport layer.
//
// Given that DatagramClientSocket class contains numerous methods tailored for
// UDP, many methods from DatagramClientSocket are left as stubs.
// ConnectViaStream is used to connect this socket over the provided QUIC stream
// to send and receive datagrams.
class NET_EXPORT_PRIVATE QuicProxyDatagramClientSocket
    : public DatagramClientSocket {
 public:
  // Initializes a QuicProxyDatagramClientSocket with the provided network
  // log (source_net_log) and destination URL. The destination parameter
  // specifies the target server (including the scheme, hostname, and port
  // number) to which datagrams will be sent.
  QuicProxyDatagramClientSocket(const NetLogWithSource& source_net_log,
                                url::SchemeHostPort destination);

  QuicProxyDatagramClientSocket(const QuicProxyDatagramClientSocket&) = delete;
  QuicProxyDatagramClientSocket& operator=(
      const QuicProxyDatagramClientSocket&) = delete;

  ~QuicProxyDatagramClientSocket() override;

  // Connect this socket over the given QUIC stream, using the `destination_`
  // and local and proxy peer addresses. The socket has no true peer
  // address since it is connected over a proxy and the proxy performs the
  // hostname resolution. Instead `proxy_peer_address_` is the peer to which the
  // underlying socket is connected.
  int ConnectViaStream(const IPEndPoint& local_address,
                       const IPEndPoint& proxy_peer_address,
                       std::unique_ptr<QuicChromiumClientStream::Handle> stream,
                       CompletionOnceCallback callback);

  // DatagramClientSocket implementation.
  int Connect(const IPEndPoint& address) override;
  int ConnectUsingNetwork(handles::NetworkHandle network,
                          const IPEndPoint& address) override;
  int ConnectUsingDefaultNetwork(const IPEndPoint& address) override;
  int ConnectAsync(const IPEndPoint& address,
                   CompletionOnceCallback callback) override;
  int ConnectUsingNetworkAsync(handles::NetworkHandle network,
                               const IPEndPoint& address,
                               CompletionOnceCallback callback) override;
  int ConnectUsingDefaultNetworkAsync(const IPEndPoint& address,
                                      CompletionOnceCallback callback) override;
  handles::NetworkHandle GetBoundNetwork() const override;
  void ApplySocketTag(const SocketTag& tag) override;
  int SetMulticastInterface(uint32_t interface_index) override;
  void SetIOSNetworkServiceType(int ios_network_service_type) override;

  // DatagramSocket implementation.
  void Close() override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  void UseNonBlockingIO() override;
  int SetDoNotFragment() override;
  int SetRecvTos() override;
  void SetMsgConfirm(bool confirm) override;
  const NetLogWithSource& NetLog() const override;

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

 private:
  NetLogWithSource net_log_;
  IPEndPoint local_address_;
  IPEndPoint proxy_peer_address_;
  std::unique_ptr<QuicChromiumClientStream::Handle> stream_;
  url::SchemeHostPort destination_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PROXY_DATAGRAM_CLIENT_SOCKET_H_

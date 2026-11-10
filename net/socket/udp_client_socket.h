// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UDP_CLIENT_SOCKET_H_
#define NET_SOCKET_UDP_CLIENT_SOCKET_H_

#include <stdint.h>

#include "net/base/net_export.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/udp_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class NetLog;
struct NetLogSource;

// A client socket that uses UDP as the transport layer.
class NET_EXPORT_PRIVATE UDPClientSocket final : public DatagramClientSocket {
 public:
  // If `target_network` is not `handles::kInvalidNetworkHandle`, the socket
  // will be bound to it. All data traffic on the socket will be sent and
  // received via `target_network`. Communication using this socket will fail if
  // `target_network` disconnects.
  UDPClientSocket(DatagramSocket::BindType bind_type,
                  net::NetLog* net_log,
                  const net::NetLogSource& source,
                  handles::NetworkHandle target_network);

  UDPClientSocket(DatagramSocket::BindType bind_type,
                  NetLogWithSource source_net_log,
                  handles::NetworkHandle target_network);

  UDPClientSocket(const UDPClientSocket&) = delete;
  UDPClientSocket& operator=(const UDPClientSocket&) = delete;

  ~UDPClientSocket() override;

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
  DscpAndEcn GetLastTos() const override;

  handles::NetworkHandle GetBoundNetwork() const override;
  void ApplySocketTag(const SocketTag& tag) override;
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  base::expected<DatagramsMetadata, Error> ReadMultiple(
      IOBuffer* buf,
      size_t buf_len,
      size_t maximum_packet_size,
      base::OnceCallback<void(base::expected<DatagramsMetadata, Error>)>
          callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;

  void Close() override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  // Switch to use non-blocking IO. Must be called right after construction and
  // before other calls.
  void UseNonBlockingIO() override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int SetDoNotFragment() override;
  int SetRecvTos() override;
  int SetTos(DiffServCodePoint dscp, EcnCodePoint ecn) override;
  void SetMsgConfirm(bool confirm) override;
  const NetLogWithSource& NetLog() const override;
  void EnableRecvOptimization() override;

  int SetMulticastInterface(uint32_t interface_index) override;
  void SetIOSNetworkServiceType(int ios_network_service_type) override;
  void RegisterQuicConnectionClosePayload(base::span<uint8_t> payload) override;
  void UnregisterQuicConnectionClosePayload() override;
  void AllowEmulatedConnect(bool allow) override;

  // Takes ownership of an opened but unconnected and unbound `socket`. This
  // method must be called after UseNonBlockingIO, otherwise the adopted socket
  // will not have the non-blocking IO flag set.
  int AdoptOpenedSocket(AddressFamily address_family, SocketDescriptor socket);

  uint32_t get_multicast_interface_for_testing() {
    return socket_.get_multicast_interface_for_testing();
  }
#if !BUILDFLAG(IS_WIN)
  bool get_msg_confirm_for_testing() {
    return socket_.get_msg_confirm_for_testing();
  }
  bool get_recv_optimization_for_testing() {
    return socket_.get_experimental_recv_optimization_enabled_for_testing();
  }
#endif
#if BUILDFLAG(IS_WIN)
  bool get_use_non_blocking_io_for_testing() {
    return socket_.get_use_non_blocking_io_for_testing();
  }
#endif

 private:
  NetLogWithSource net_log_;
  UDPSocket socket_;
  bool adopted_opened_socket_ = false;
  bool connect_called_ = false;
  // The network the socket is currently bound to.
  // TODO(crbug.com/518753285): Once QUIC is no longer handling bindToNetwork
  // calls in a unique way, merge `network_` and `connect_using_network_`
  // into a single `target_network_`.
  handles::NetworkHandle network_ = handles::kInvalidNetworkHandle;
  handles::NetworkHandle connect_using_network_ =
      handles::kInvalidNetworkHandle;
};

}  // namespace net

#endif  // NET_SOCKET_UDP_CLIENT_SOCKET_H_

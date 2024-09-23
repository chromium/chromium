// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_BROKERED_UDP_CLIENT_SOCKET_H_
#define SERVICES_NETWORK_BROKERED_UDP_CLIENT_SOCKET_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_handle.h"
#include "net/log/net_log_source.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/socket_tag.h"
#include "net/socket/stream_socket.h"
#include "net/socket/udp_client_socket.h"
#include "net/socket/udp_socket.h"
#include "net/socket/udp_socket_global_limits.h"

#include "services/network/broker_helper_win.h"

namespace net {
class IOBuffer;
class IPEndPoint;
class NetLog;
}  // namespace net

namespace network {

class BrokeredClientSocketFactory;
class TransferableSocket;

// A client socket used exclusively with a socket broker. Currently intended for
// Windows only. Not intended to be used by non-brokered connections. Generally,
// all calls pass through to an underlying TCPClientSocket API, but Bind and
// Connect are the sent to a privileged process using the net:SocketBroker
// interface. This is because socket creation needs to be brokered, and
// TCPClientSocket only creates and opens a socket within Bind and Connect.
class COMPONENT_EXPORT(NETWORK_SERVICE) BrokeredUdpClientSocket
    : public net::DatagramClientSocket {
 public:
  BrokeredUdpClientSocket(net::DatagramSocket::BindType bind_type,
                          net::NetLog* net_log,
                          const net::NetLogSource& source,
                          BrokeredClientSocketFactory* client_socket_factory,
                          net::handles::NetworkHandle network =
                              net::handles::kInvalidNetworkHandle);

  ~BrokeredUdpClientSocket() override;

  BrokeredUdpClientSocket(const BrokeredUdpClientSocket&) = delete;
  BrokeredUdpClientSocket& operator=(const BrokeredUdpClientSocket&) = delete;

  // DatagramClientSocket implementation.
  // TODO(crbug.com/40267879): Remove Connect, ConnectUsingNetwork, and
  // ConnectUsingDefaultNetwork once consumers have been migrated to only call
  // Connect*Async methods.
  int Connect(const net::IPEndPoint& address) override;
  int ConnectUsingNetwork(net::handles::NetworkHandle network,
                          const net::IPEndPoint& address) override;
  int ConnectUsingDefaultNetwork(const net::IPEndPoint& address) override;
  int ConnectAsync(const net::IPEndPoint& address,
                   net::CompletionOnceCallback callback) override;
  int ConnectUsingNetworkAsync(net::handles::NetworkHandle network,
                               const net::IPEndPoint& address,
                               net::CompletionOnceCallback callback) override;
  int ConnectUsingDefaultNetworkAsync(
      const net::IPEndPoint& address,
      net::CompletionOnceCallback callback) override;
  net::handles::NetworkHandle GetBoundNetwork() const override;
  void ApplySocketTag(const net::SocketTag& tag) override;
  void EnableRecvOptimization() override;
  int SetMulticastInterface(uint32_t interface_index) override;
  void SetIOSNetworkServiceType(int ios_network_service_type) override;
  net::DscpAndEcn GetLastTos() const override;

  // DatagramSocket implementation.
  void Close() override;
  int GetPeerAddress(net::IPEndPoint* address) const override;
  int GetLocalAddress(net::IPEndPoint* address) const override;
  // Switch to use non-blocking IO. Must be called right after construction and
  // before other calls.
  void UseNonBlockingIO() override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int SetDoNotFragment() override;
  int SetRecvTos() override;
  int SetTos(net::DiffServCodePoint dscp, net::EcnCodePoint ecn) override;
  void SetMsgConfirm(bool confirm) override;
  const net::NetLogWithSource& NetLog() const override;

  // Socket implementation.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int Write(
      net::IOBuffer* buf,
      int buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

  uint32_t get_multicast_interface_for_testing() {
    return socket_->get_multicast_interface_for_testing();
  }
#if !BUILDFLAG(IS_WIN)
  bool get_msg_confirm_for_testing() {
    return socket_->get_msg_confirm_for_testing();
  }
  bool get_recv_optimization_for_testing() {
    return socket_->get_recv_optimization_for_testing();
  }
#endif
#if BUILDFLAG(IS_WIN)
  bool get_use_non_blocking_io_for_testing() {
    return socket_->get_use_non_blocking_io_for_testing();
  }
  void SetBrokerHelperDelegateForTesting(
      std::unique_ptr<BrokerHelperWin::Delegate> delegate) {
    broker_helper_.SetDelegateForTesting(std::move(delegate));
  }
#endif

 private:
  // On Windows, this method determines if a Connection needs to be brokered.
  // Directly creates a new `socket_` if brokering is not required, calls
  // `BrokerCreateUdpSocket` if it is.
  int ConnectAsyncInternal(const net::IPEndPoint& address,
                           net::CompletionOnceCallback callback);
  // Synchronously creates and connects a socket. This method can only be used
  // on Windows if a connection does not need to be brokered.
  int ConnectInternal(const net::IPEndPoint& address);
  // Returns a net error result upon opening and connecting `socket_`. If a
  // connection needs to be brokered, the return value is ignored as callback is
  // run with the return value instead.
  int DidCompleteCreate(bool should_broker,
                        const net::IPEndPoint& address,
                        net::CompletionOnceCallback callback,
                        network::TransferableSocket socket,
                        int result);

  net::DatagramSocket::BindType bind_type_;
  net::handles::NetworkHandle network_;
  net::NetLogWithSource net_log_source_;
  // Need to store the tag in case ApplySocketTag() is called before Connect().
  net::SocketTag tag_;
  uint32_t interface_index_ = 0;
  bool use_non_blocking_io_ = false;
  bool set_msg_confirm_ = false;
  bool connect_called_ = false;
  bool recv_optimization_ = false;

  // The underlying brokered socket. Created when the socket is created for
  // Connect().
  std::unique_ptr<net::UDPClientSocket> socket_;

  // The ClientSocketFactory that created this socket. Used to send IPCs to the
  // remote SocketBroker.
  const raw_ptr<BrokeredClientSocketFactory> client_socket_factory_;

  BrokerHelperWin broker_helper_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BrokeredUdpClientSocket> brokered_weak_ptr_factory_{
      this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_BROKERED_UDP_CLIENT_SOCKET_H_

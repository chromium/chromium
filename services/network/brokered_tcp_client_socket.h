// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_BROKERED_TCP_CLIENT_SOCKET_H_
#define SERVICES_NETWORK_BROKERED_TCP_CLIENT_SOCKET_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/socket/socket_tag.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_socket.h"
#include "net/socket/transport_client_socket.h"

namespace net {

class NetLog;
struct NetLogSource;
class SocketPerformanceWatcher;
class NetworkQualityEstimator;
class SocketTag;
}  // namespace net

namespace network {

class BrokeredClientSocketFactory;
class TransferableSocket;

// A client socket used exclusively with a socket broker. Currently intended for
// Windows and Android only. Not intended to be used by non-brokered
// connections. Generally, all calls pass through to an underlying
// TCPClientSocket API, but Bind and Connect are the sent to a privileged
// process using the net:SocketBroker interface. This is because socket creation
// needs to be brokered, and TCPClientSocket only creates and opens a socket
// within Bind and Connect.
class COMPONENT_EXPORT(NETWORK_SERVICE) BrokeredTcpClientSocket
    : public net::TransportClientSocket {
 public:
  BrokeredTcpClientSocket(
      const net::AddressList& addresses,
      std::unique_ptr<net::SocketPerformanceWatcher>
          brokered_socket_performance_watcher,
      net::NetworkQualityEstimator* network_quality_estimator,
      net::NetLog* net_log,
      const net::NetLogSource& source,
      BrokeredClientSocketFactory* client_socket_factory);

  ~BrokeredTcpClientSocket() override;

  BrokeredTcpClientSocket(const BrokeredTcpClientSocket&) = delete;
  BrokeredTcpClientSocket& operator=(const BrokeredTcpClientSocket&) = delete;

  // TransportClientSocket implementation.
  int Bind(const net::IPEndPoint& address) override;
  bool SetKeepAlive(bool enable, int delay) override;
  bool SetNoDelay(bool no_delay) override;

  // StreamSocket implementation.
  void SetBeforeConnectCallback(
      const BeforeConnectCallback& before_connect_callback) override;
  int Connect(net::CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(net::IPEndPoint* address) const override;
  int GetLocalAddress(net::IPEndPoint* address) const override;
  const net::NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  net::NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(net::SSLInfo* ssl_info) override;
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const net::SocketTag& tag) override;

  // Socket implementation.
  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int ReadIfReady(net::IOBuffer* buf,
                  int buf_len,
                  net::CompletionOnceCallback callback) override;
  int CancelReadIfReady() override;
  int Write(
      net::IOBuffer* buf,
      int buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

 private:
  void DidCompleteOpenForBind(const net::IPEndPoint& address,
                              std::unique_ptr<net::TCPSocket> new_socket,
                              net::Error result);

  void DidCompleteConnect(net::CompletionOnceCallback callback, int result);

  void DidCompleteCreate(net::CompletionOnceCallback callback,
                         network::TransferableSocket socket,
                         int result);

  // The list of addresses we should try in order to establish a connection.
  net::AddressList addresses_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Arguments for creating a new TCPClientSocket
  std::unique_ptr<net::SocketPerformanceWatcher> socket_performance_watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);

  raw_ptr<net::NetworkQualityEstimator> network_quality_estimator_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const net::NetLogWithSource net_log_source_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // State to track whether socket is currently attempting to connect.
  bool is_connect_in_progress_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Local IP address and port we are bound to. Set to NULL if Bind()
  // wasn't called (in that case OS chooses address/port).
  std::unique_ptr<net::IPEndPoint> bind_address_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  BeforeConnectCallback before_connect_callback_;

  // Need to store the tag in case ApplySocketTag() is called before Connect().
  net::SocketTag tag_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The underlying brokered socket. Created when the socket is created for
  // Connect().
  std::unique_ptr<net::TransportClientSocket> brokered_socket_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The ClientSocketFactory that created this socket. Used to send IPCs to the
  // remote SocketBroker.
  const raw_ptr<BrokeredClientSocketFactory> client_socket_factory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BrokeredTcpClientSocket> brokered_weak_ptr_factory_{
      this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_BROKERED_TCP_CLIENT_SOCKET_H_

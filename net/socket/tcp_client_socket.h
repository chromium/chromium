// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_CLIENT_SOCKET_H_
#define NET_SOCKET_TCP_CLIENT_SOCKET_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_socket.h"
#include "net/socket/transport_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class NetLog;
struct NetLogSource;
class SocketPerformanceWatcher;

// A client socket that uses TCP as the transport layer.
class NET_EXPORT TCPClientSocket : public TransportClientSocket {
 public:
  // The IP address(es) and port number to connect to.  The TCP socket will try
  // each IP address in the list until it succeeds in establishing a
  // connection.
  TCPClientSocket(
      const AddressList& addresses,
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      net::NetLog* net_log,
      const net::NetLogSource& source);

  // Adopts the given, connected socket and then acts as if Connect() had been
  // called. This function is used by TCPServerSocket and for testing.
  TCPClientSocket(std::unique_ptr<TCPSocket> connected_socket,
                  const IPEndPoint& peer_address);

  // Creates a TCPClientSocket from a bound-but-not-connected socket.
  static std::unique_ptr<TCPClientSocket> CreateFromBoundSocket(
      std::unique_ptr<TCPSocket> bound_socket,
      const AddressList& addresses,
      const IPEndPoint& bound_address);

  ~TCPClientSocket() override;

  // TransportClientSocket implementation.
  int Bind(const IPEndPoint& address) override;
  bool SetKeepAlive(bool enable, int delay) override;
  bool SetNoDelay(bool no_delay) override;

  // StreamSocket implementation.
  void SetBeforeConnectCallback(
      const BeforeConnectCallback& before_connect_callback) override;
  int Connect(CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  const NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  void EnableTCPFastOpenIfSupported() override;
  bool WasAlpnNegotiated() const override;
  NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  void GetConnectionAttempts(ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override;
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override;
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const SocketTag& tag) override;

  // Socket implementation.
  // Multiple outstanding requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override;
  int CancelReadIfReady() override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

  // Exposes the underlying socket descriptor for testing its state. Does not
  // release ownership of the descriptor.
  SocketDescriptor SocketDescriptorForTesting() const;

 private:
  // State machine for connecting the socket.
  enum ConnectState {
    CONNECT_STATE_CONNECT,
    CONNECT_STATE_CONNECT_COMPLETE,
    CONNECT_STATE_NONE,
  };

  // Main constructor. |socket| must be non-null. |current_address_index| is the
  // address index in |addresses| of the server |socket| is connected to, or -1
  // if not connected. |bind_address|, if present, is the address |socket| is
  // bound to.
  TCPClientSocket(std::unique_ptr<TCPSocket> socket,
                  const AddressList& addresses,
                  int current_address_index,
                  std::unique_ptr<IPEndPoint> bind_address);

  // A helper method shared by Read() and ReadIfReady(). If |read_if_ready| is
  // set to true, ReadIfReady() will be used instead of Read().
  int ReadCommon(IOBuffer* buf,
                 int buf_len,
                 const CompletionOnceCallback callback,
                 bool read_if_ready);

  // State machine used by Connect().
  int DoConnectLoop(int result);
  int DoConnect();
  int DoConnectComplete(int result);

  // Helper used by Disconnect(), which disconnects minus resetting
  // current_address_index_ and bind_address_.
  void DoDisconnect();

  void DidCompleteConnect(int result);
  void DidCompleteRead(CompletionOnceCallback callback, int result);
  void DidCompleteWrite(CompletionOnceCallback callback, int result);
  void DidCompleteReadWrite(CompletionOnceCallback callback, int result);

  int OpenSocket(AddressFamily family);

  // Emits histograms for TCP metrics, at the time the socket is
  // disconnected.
  void EmitTCPMetricsHistogramsOnDisconnect();

  std::unique_ptr<TCPSocket> socket_;

  // Local IP address and port we are bound to. Set to NULL if Bind()
  // wasn't called (in that case OS chooses address/port).
  std::unique_ptr<IPEndPoint> bind_address_;

  // The list of addresses we should try in order to establish a connection.
  AddressList addresses_;

  // Where we are in above list. Set to -1 if uninitialized.
  int current_address_index_;

  // External callback; called when connect is complete.
  CompletionOnceCallback connect_callback_;

  // The next state for the Connect() state machine.
  ConnectState next_connect_state_;

  // This socket was previously disconnected and has not been re-connected.
  bool previously_disconnected_;

  // Failed connection attempts made while trying to connect this socket.
  ConnectionAttempts connection_attempts_;

  // Total number of bytes received by the socket.
  int64_t total_received_bytes_;

  BeforeConnectCallback before_connect_callback_;

  bool was_ever_used_;

  DISALLOW_COPY_AND_ASSIGN(TCPClientSocket);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_CLIENT_SOCKET_H_

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_CLIENT_SOCKET_H_
#define NET_SOCKET_TCP_CLIENT_SOCKET_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_socket.h"
#include "net/socket/transport_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

// PowerMonitor exists on Android, but doesn't get suspend mode signals. It
// doesn't exist in NaCl, so don't use it to watch for suspend events on those
// platforms.
#if !defined(OS_ANDROID) && !defined(OS_NACL)
// Define SOCKETS_OBSERVE_SUSPEND if sockets should watch for suspend events so
// they can fail pending socket operations on suspend. Otherwise, connections
// hang for varying lengths of time when leaving suspend mode before failing
// with TCP keepalive errors (~1 minute on macOS 10.14, up to several minutes on
// Windows 10 1803). Firefox doesn't seems to need this logic, for unclear
// reasons (experimentally, it doesn't seem to be the differences in the keep
// alive settings it sets TCP sockets).
#define TCP_CLIENT_SOCKET_OBSERVES_SUSPEND
#endif

namespace net {

class IPEndPoint;
class NetLog;
struct NetLogSource;
class SocketPerformanceWatcher;

// A client socket that uses TCP as the transport layer.
class NET_EXPORT TCPClientSocket : public TransportClientSocket,
                                   public base::PowerObserver {
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

  // base::PowerObserver methods:
  void OnSuspend() override;

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

  // Calls the connect method of |socket_|. Used in tests, to ensure a socket
  // never connects.
  virtual int ConnectInternal(const IPEndPoint& endpoint);

  // Helper used by Disconnect(), which disconnects minus resetting
  // current_address_index_ and bind_address_.
  void DoDisconnect();

  void DidCompleteConnect(int result);
  void DidCompleteRead(int result);
  void DidCompleteWrite(int result);
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

  // External callbacks; called when corresponding operations are complete.
  // Cleared when no such operation is pending.
  CompletionOnceCallback connect_callback_;
  CompletionOnceCallback read_callback_;
  CompletionOnceCallback write_callback_;

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

  // Set to true if the socket was disconnected due to entering suspend mode.
  // Once set, read/write operations return ERR_NETWORK_IO_SUSPENDED, until
  // Connect() or Disconnect() is called.
  bool was_disconnected_on_suspend_;

  base::WeakPtrFactory<TCPClientSocket> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TCPClientSocket);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_CLIENT_SOCKET_H_

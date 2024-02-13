// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_SERVER_SOCKET_H_
#define NET_SOCKET_TCP_SERVER_SOCKET_H_

#include <memory>

#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/socket/server_socket.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/tcp_socket.h"

namespace net {

class NetLog;
struct NetLogSource;

// A server socket that uses TCP as the transport layer.
class NET_EXPORT TCPServerSocket : public ServerSocket {
 public:
  TCPServerSocket(NetLog* net_log, const NetLogSource& source);

  // Adopts the provided socket, which must not be a connected socket.
  explicit TCPServerSocket(std::unique_ptr<TCPSocket> socket);

  TCPServerSocket(const TCPServerSocket&) = delete;
  TCPServerSocket& operator=(const TCPServerSocket&) = delete;

  ~TCPServerSocket() override;

  // Takes ownership of |socket|, which has been opened, but may or may not be
  // bound or listening. The caller must determine this based on the provenance
  // of the socket and act accordingly. The socket may have connections waiting
  // to be accepted, but must not be actually connected.
  int AdoptSocket(SocketDescriptor socket);

  // net::ServerSocket implementation.
  int Listen(const IPEndPoint& address,
             int backlog,
             std::optional<bool> ipv6_only) override;
  int GetLocalAddress(IPEndPoint* address) const override;
  int Accept(std::unique_ptr<StreamSocket>* socket,
             CompletionOnceCallback callback) override;
  int Accept(std::unique_ptr<StreamSocket>* socket,
             CompletionOnceCallback callback,
             IPEndPoint* peer_address) override;

  // Detaches from the current thread, to allow the socket to be transferred to
  // a new thread. Should only be called when the object is no longer used by
  // the old thread.
  void DetachFromThread();

 private:
  // Converts |accepted_socket_| and stores the result in
  // |output_accepted_socket|.
  // |output_accepted_socket| is untouched on failure. But |accepted_socket_| is
  // set to NULL in any case.
  int ConvertAcceptedSocket(
      int result,
      std::unique_ptr<StreamSocket>* output_accepted_socket,
      IPEndPoint* output_accepted_address);
  // Completion callback for calling TCPSocket::Accept().
  void OnAcceptCompleted(std::unique_ptr<StreamSocket>* output_accepted_socket,
                         IPEndPoint* output_accepted_address,
                         CompletionOnceCallback forward_callback,
                         int result);

  std::unique_ptr<TCPSocket> socket_;

  std::unique_ptr<TCPSocket> accepted_socket_;
  IPEndPoint accepted_address_;
  bool pending_accept_ = false;
  bool adopted_opened_socket_ = false;
};

}  // namespace net

#endif  // NET_SOCKET_TCP_SERVER_SOCKET_H_

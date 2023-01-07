// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TRANSPORT_CLIENT_SOCKET_H_
#define NET_SOCKET_TRANSPORT_CLIENT_SOCKET_H_

#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/stream_socket.h"

namespace net {

// A socket class that extends StreamSocket to provide methods that are relevant
// to a transport client socket.
class NET_EXPORT TransportClientSocket : public StreamSocket {
 public:
  TransportClientSocket();

  TransportClientSocket(const TransportClientSocket&) = delete;
  TransportClientSocket& operator=(const TransportClientSocket&) = delete;

  ~TransportClientSocket() override;

  // Binds the socket to a local address, |local_addr|. Returns OK on success,
  // and a net error code on failure.
  virtual int Bind(const net::IPEndPoint& local_addr) = 0;

  // Enables/disables buffering in the kernel. By default, on Linux, TCP sockets
  // will wait up to 200ms for more data to complete a packet before
  // transmitting. After calling this function, the kernel will not wait. See
  // TCP_NODELAY in `man 7 tcp`. On Windows, the Nagle implementation is
  // governed by RFC 896. SetTCPNoDelay() sets the TCP_NODELAY option. Use
  // |no_delay| to enable or disable it. Returns true on success, and false on
  // failure.
  //
  // Should not be called and will always fail until there is not an underlying
  // platform socket ready to receive options. The underlying platform socket
  // should always be ready after successful connection or slightly earlier
  // during BeforeConnect handlers.
  virtual bool SetNoDelay(bool no_delay);

  // Enables or disables TCP Keep-Alive (which is the SO_KEEPALIVE option on the
  // socket). The unit for the delay is in seconds. Returns true on success, and
  // false on failure.
  //
  // Should not be called and will always fail until there is not an underlying
  // platform socket ready to receive options. The underlying platform socket
  // should always be ready after successful connection or slightly earlier
  // during BeforeConnect handlers.
  virtual bool SetKeepAlive(bool enable, int delay_secs);
};

}  // namespace net

#endif  // NET_SOCKET_TRANSPORT_CLIENT_SOCKET_H_

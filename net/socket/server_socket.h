// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SERVER_SOCKET_H_
#define NET_SOCKET_SERVER_SOCKET_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"

namespace net {

class IPEndPoint;
class StreamSocket;

class NET_EXPORT ServerSocket {
 public:
  ServerSocket();

  ServerSocket(const ServerSocket&) = delete;
  ServerSocket& operator=(const ServerSocket&) = delete;

  virtual ~ServerSocket();

  // Binds the socket and starts listening. Destroys the socket to stop
  // listening.
  // |ipv6_only| can be used by inheritors to control whether the server listens
  // on IPv4/IPv6 or just IPv6 -- |true| limits connections to IPv6 only,
  // |false| allows both IPv4/IPv6 connections; leaving the value unset implies
  // default behavior (|true| on Windows, |false| on Posix).
  virtual int Listen(const IPEndPoint& address,
                     int backlog,
                     std::optional<bool> ipv6_only) = 0;

  // Binds the socket with address and port, and starts listening. It expects
  // a valid IPv4 or IPv6 address. Otherwise, it returns ERR_ADDRESS_INVALID.
  virtual int ListenWithAddressAndPort(const std::string& address_string,
                                       uint16_t port,
                                       int backlog);

  // Gets current address the socket is bound to.
  virtual int GetLocalAddress(IPEndPoint* address) const = 0;

  // Accepts connection. Callback is called when new connection is
  // accepted.
  virtual int Accept(std::unique_ptr<StreamSocket>* socket,
                     CompletionOnceCallback callback) = 0;

  // Accepts connection. Callback is called when new connection is accepted.
  // Note: |peer_address| may or may not be populated depending on the
  // implementation.
  virtual int Accept(std::unique_ptr<StreamSocket>* socket,
                     CompletionOnceCallback callback,
                     IPEndPoint* peer_address);
};

}  // namespace net

#endif  // NET_SOCKET_SERVER_SOCKET_H_

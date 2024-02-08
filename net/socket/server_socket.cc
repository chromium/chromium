// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/server_socket.h"

#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"

namespace net {

ServerSocket::ServerSocket() = default;

ServerSocket::~ServerSocket() = default;

int ServerSocket::ListenWithAddressAndPort(const std::string& address_string,
                                           uint16_t port,
                                           int backlog) {
  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(address_string)) {
    return ERR_ADDRESS_INVALID;
  }

  return Listen(IPEndPoint(ip_address, port), backlog,
                /*ipv6_only=*/std::nullopt);
}

int ServerSocket::Accept(std::unique_ptr<StreamSocket>* socket,
                         net::CompletionOnceCallback callback,
                         net::IPEndPoint* peer_address) {
  if (peer_address) {
    *peer_address = IPEndPoint();
  }
  return Accept(socket, std::move(callback));
}

}  // namespace net

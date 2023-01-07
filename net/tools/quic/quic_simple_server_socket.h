// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_SIMPLE_SERVER_SOCKET_H_
#define NET_TOOLS_QUIC_QUIC_SIMPLE_SERVER_SOCKET_H_

#include "net/base/ip_endpoint.h"
#include "net/socket/udp_server_socket.h"

namespace net {

// Creates a UDP server socket tuned for use in a QUIC server.
std::unique_ptr<UDPServerSocket> CreateQuicSimpleServerSocket(
    const IPEndPoint& address,
    IPEndPoint* server_address);

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SIMPLE_SERVER_SOCKET_H_

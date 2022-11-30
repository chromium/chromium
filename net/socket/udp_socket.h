// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UDP_SOCKET_H_
#define NET_SOCKET_UDP_SOCKET_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "net/socket/udp_socket_win.h"
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "net/socket/udp_socket_posix.h"
#endif

namespace net {

// UDPSocket
// Accessor API for a UDP socket in either client or server form.
//
// Client form:
// In this case, we're connecting to a specific server, so the client will
// usually use:
//       Open(address_family)  // Open a socket.
//       Connect(address)      // Connect to a UDP server
//       Read/Write            // Reads/Writes all go to a single destination
//
// Server form:
// In this case, we want to read/write to many clients which are connecting
// to this server.  First the server 'binds' to an addres, then we read from
// clients and write responses to them.
// Example:
//       Open(address_family)  // Open a socket.
//       Bind(address/port)    // Binds to port for reading from clients
//       RecvFrom/SendTo       // Each read can come from a different client
//                             // Writes need to be directed to a specific
//                             // address.
#if BUILDFLAG(IS_WIN)
typedef UDPSocketWin UDPSocket;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
typedef UDPSocketPosix UDPSocket;
#endif

}  // namespace net

#endif  // NET_SOCKET_UDP_SOCKET_H_

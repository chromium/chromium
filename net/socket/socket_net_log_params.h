// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_NET_LOG_PARAMS_H_
#define NET_SOCKET_SOCKET_NET_LOG_PARAMS_H_

#include "net/base/sys_addrinfo.h"
#include "net/log/net_log_event_type.h"

namespace base {
class Value;
}

namespace net {

class NetLogWithSource;
class HostPortPair;
class IPEndPoint;

// Emits an event to NetLog with socket error parameters.
void NetLogSocketError(const NetLogWithSource& net_log,
                       NetLogEventType type,
                       int net_error,
                       int os_error);

// Creates a NetLog parameters for a HostPortPair.
base::Value CreateNetLogHostPortPairParams(const HostPortPair* host_and_port);

// Creates a NetLog parameters for an IPEndPoint.
base::Value CreateNetLogIPEndPointParams(const IPEndPoint* address);

// Creates a NetLog parameters for the source sockaddr on connect events.
base::Value CreateNetLogSourceAddressParams(const struct sockaddr* net_address,
                                            socklen_t address_len);

}  // namespace net

#endif  // NET_SOCKET_SOCKET_NET_LOG_PARAMS_H_

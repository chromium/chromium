// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_NET_LOG_PARAMS_H_
#define NET_SOCKET_SOCKET_NET_LOG_PARAMS_H_

#include "base/values.h"
#include "net/log/net_log_event_type.h"

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
base::Value::Dict CreateNetLogHostPortPairParams(
    const HostPortPair* host_and_port);

// Creates a NetLog parameters for an IPEndPoint.
base::Value::Dict CreateNetLogIPEndPointParams(const IPEndPoint* address);

// Creates a NetLog parameters for the local and remote IPEndPoints on connect
// events.
base::Value::Dict CreateNetLogAddressPairParams(
    const net::IPEndPoint& local_address,
    const net::IPEndPoint& remote_address);

}  // namespace net

#endif  // NET_SOCKET_SOCKET_NET_LOG_PARAMS_H_

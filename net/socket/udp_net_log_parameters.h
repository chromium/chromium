// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UDP_NET_LOG_PARAMETERS_H_
#define NET_SOCKET_UDP_NET_LOG_PARAMETERS_H_

#include "base/values.h"
#include "net/base/network_handle.h"
#include "net/log/net_log_event_type.h"

namespace net {

class NetLogWithSource;
class IPEndPoint;

// Emits a NetLog event with parameters describing a UDP receive/send event.
// |bytes| are only logged when byte logging is enabled.  |address| may be
// nullptr.
void NetLogUDPDataTransfer(const NetLogWithSource& net_log,
                           NetLogEventType type,
                           int byte_count,
                           const char* bytes,
                           const IPEndPoint* address);

// Creates NetLog parameters describing a UDP connect event.
base::Value::Dict CreateNetLogUDPConnectParams(const IPEndPoint& address,
                                               handles::NetworkHandle network);

}  // namespace net

#endif  // NET_SOCKET_UDP_NET_LOG_PARAMETERS_H_

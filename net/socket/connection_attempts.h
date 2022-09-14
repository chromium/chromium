// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CONNECTION_ATTEMPTS_H_
#define NET_SOCKET_CONNECTION_ATTEMPTS_H_

#include "net/base/ip_endpoint.h"

namespace net {

// A record of an connection attempt made to connect to a host. Includes TCP
// and SSL errors, but not proxy connections.
struct ConnectionAttempt {
  ConnectionAttempt(const IPEndPoint endpoint, int result)
      : endpoint(endpoint), result(result) {}

  bool operator==(const ConnectionAttempt& other) const {
    return endpoint == other.endpoint && result == other.result;
  }

  // Address and port the socket layer attempted to connect to.
  IPEndPoint endpoint;

  // Net error indicating the result of that attempt.
  int result;
};

// Multiple connection attempts, as might be tracked in an HttpTransaction or a
// URLRequest. Order is insignificant.
typedef std::vector<ConnectionAttempt> ConnectionAttempts;

}  // namespace net

#endif  // NET_SOCKET_CONNECTION_ATTEMPTS_H_

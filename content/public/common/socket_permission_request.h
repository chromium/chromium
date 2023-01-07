// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SOCKET_PERMISSION_REQUEST_H_
#define CONTENT_PUBLIC_COMMON_SOCKET_PERMISSION_REQUEST_H_

#include <stdint.h>

#include <string>


namespace content {

// This module provides helper types for checking socket permission.

struct SocketPermissionRequest {
  enum OperationType {
    NONE = 0,
    TCP_CONNECT,
    TCP_LISTEN,
    UDP_BIND,
    UDP_SEND_TO,
    UDP_MULTICAST_MEMBERSHIP,
    RESOLVE_HOST,
    RESOLVE_PROXY,
    NETWORK_STATE,
    OPERATION_TYPE_LAST = NETWORK_STATE
  };

  SocketPermissionRequest(OperationType type,
                          const std::string& host,
                          uint16_t port)
      : type(type), host(host), port(port) {}

  OperationType type;
  std::string host;
  uint16_t port;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SOCKET_PERMISSION_REQUEST_H_

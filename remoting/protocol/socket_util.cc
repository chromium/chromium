// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/socket_util.h"

#include "net/base/net_errors.h"

namespace remoting {

SocketErrorAction GetSocketErrorAction(int error) {
  switch (error) {
    // UDP is connectionless, so we may receive ICMP unreachable or reset errors
    // for previous sends to different addresses.
    case net::ERR_ADDRESS_UNREACHABLE:
    case net::ERR_CONNECTION_RESET:
      return SOCKET_ERROR_ACTION_RETRY;

    // Target address is invalid. The socket is still usable for different
    // target addresses and the error can be ignored.
    case net::ERR_ADDRESS_INVALID:
      return SOCKET_ERROR_ACTION_IGNORE;

    // May be returned when the packet is blocked by local firewall (see
    // https://code.google.com/p/webrtc/issues/detail?id=1207). The firewall may
    // still allow us to send to other addresses, so ignore the error for this
    // particular send.
    case net::ERR_ACCESS_DENIED:
      return SOCKET_ERROR_ACTION_IGNORE;

    // Indicates that the buffer in the network adapter is full, so drop this
    // packet and assume the socket is still usable.
    case net::ERR_OUT_OF_MEMORY:
      return SOCKET_ERROR_ACTION_IGNORE;

    default:
      return SOCKET_ERROR_ACTION_FAIL;
  }
}

}  // namespace remoting

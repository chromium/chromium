// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_CHALLENGE_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_CHALLENGE_H_

#include <string>

#include "net/base/net_export.h"

namespace net {

// Given a WebSocket handshake challenge, compute the value that the server
// should return in the Sec-WebSocket-Accept header.
NET_EXPORT_PRIVATE std::string ComputeSecWebSocketAccept(
    const std::string& key);

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_CHALLENGE_H_

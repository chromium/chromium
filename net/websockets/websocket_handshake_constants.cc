// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_constants.h"

namespace net::websockets {

const char kHttpProtocolVersion[] = "HTTP/1.1";

const char kSecWebSocketProtocol[] = "Sec-WebSocket-Protocol";
const char kSecWebSocketExtensions[] = "Sec-WebSocket-Extensions";
const char kSecWebSocketKey[] = "Sec-WebSocket-Key";
const char kSecWebSocketAccept[] = "Sec-WebSocket-Accept";
const char kSecWebSocketVersion[] = "Sec-WebSocket-Version";

const char kSupportedVersion[] = "13";

const char kUpgrade[] = "Upgrade";
const char kWebSocketGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

const char kWebSocketLowercase[] = "websocket";

}  // namespace net::websockets

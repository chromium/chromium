// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_EXTENSION_PARSER_H_
#define NET_WEBSOCKETS_WEBSOCKET_EXTENSION_PARSER_H_

#include <string_view>
#include <vector>

#include "net/base/net_export.h"
#include "net/websockets/websocket_extension.h"

namespace net {

// Parses a Sec-WebSocket-Extensions header value (RFC 6455 Section 9.1).
//
// The input string must not contain newline characters, and any
// LWS-concatenation must be performed before calling this function.
//
// Returns a vector of `WebSocketExtension` objects, or an empty vector if a
// syntax error is found.
NET_EXPORT_PRIVATE std::vector<WebSocketExtension> ParseWebSocketExtensions(
    std::string_view data);

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_EXTENSION_PARSER_H_

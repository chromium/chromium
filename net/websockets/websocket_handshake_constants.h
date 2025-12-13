// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A set of common constants that are needed for the WebSocket handshake.
// In general, you should prefer using these constants to literal strings,
// except in tests.
//
// These constants cannot be used in files that are compiled on iOS, because
// this file is not compiled on iOS.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_CONSTANTS_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_CONSTANTS_H_

#include <stddef.h>

// This file plases constants inside the ::net::websockets namespace to avoid
// risk of collisions with other symbols in libnet.
namespace net::websockets {

// "HTTP/1.1"
// RFC6455 only requires HTTP/1.1 "or better" but in practice an HTTP version
// other than 1.1 should not occur in a WebSocket handshake.
inline constexpr char kHttpProtocolVersion[] = "HTTP/1.1";

// The Sec-WebSockey-Key challenge is 16 random bytes, base64 encoded.
inline constexpr size_t kRawChallengeLength = 16;

// "Sec-WebSocket-Protocol"
inline constexpr char kSecWebSocketProtocol[] = "Sec-WebSocket-Protocol";

// "Sec-WebSocket-Extensions"
inline constexpr char kSecWebSocketExtensions[] = "Sec-WebSocket-Extensions";

// "Sec-WebSocket-Key"
inline constexpr char kSecWebSocketKey[] = "Sec-WebSocket-Key";

// "Sec-WebSocket-Accept"
inline constexpr char kSecWebSocketAccept[] = "Sec-WebSocket-Accept";

// "Sec-WebSocket-Version"
inline constexpr char kSecWebSocketVersion[] = "Sec-WebSocket-Version";

// This implementation only supports one version of the WebSocket protocol,
// "13", as specified in RFC6455. If support for multiple versions is added in
// future, it will probably no longer be worth having a constant for this.
inline constexpr char kSupportedVersion[] = "13";

// "Upgrade"
inline constexpr char kUpgrade[] = "Upgrade";

// "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" as defined in section 4.1 of
// RFC6455.
inline constexpr char kWebSocketGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// "websocket", as used in the "Upgrade:" header. This is always lowercase
// (except in obsolete versions of the protocol).
inline constexpr char kWebSocketLowercase[] = "websocket";

}  // namespace net::websockets

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_CONSTANTS_H_

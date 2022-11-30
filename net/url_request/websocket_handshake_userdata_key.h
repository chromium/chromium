// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The key for WebSocket UserData in URLRequest objects. This is included in
// net/url_request because net/websockets is not linked in on iOS.

#ifndef NET_URL_REQUEST_WEBSOCKET_HANDSHAKE_USERDATA_KEY_H_
#define NET_URL_REQUEST_WEBSOCKET_HANDSHAKE_USERDATA_KEY_H_

#include "net/base/net_export.h"

namespace net {

// This variable has a unique address, which is used as the key to store
// WebSocket-handshake related data in a URLRequest object.
extern NET_EXPORT_PRIVATE const char kWebSocketHandshakeUserDataKey[];

}  // namespace net

#endif  // NET_URL_REQUEST_WEBSOCKET_HANDSHAKE_USERDATA_KEY_H_

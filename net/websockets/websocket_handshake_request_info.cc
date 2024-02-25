// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_request_info.h"

#include "base/time/time.h"

namespace net {

WebSocketHandshakeRequestInfo::WebSocketHandshakeRequestInfo(
    const GURL& url,
    base::Time request_time)
    : url(url), request_time(request_time) {}

WebSocketHandshakeRequestInfo::~WebSocketHandshakeRequestInfo() = default;

}  // namespace net

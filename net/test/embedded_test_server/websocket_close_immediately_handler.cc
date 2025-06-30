// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/websocket_close_immediately_handler.h"

namespace net::test_server {

WebSocketCloseImmediatelyHandler::WebSocketCloseImmediatelyHandler(
    scoped_refptr<net::test_server::WebSocketConnection> connection)
    : WebSocketHandler(std::move(connection)) {}

WebSocketCloseImmediatelyHandler::~WebSocketCloseImmediatelyHandler() = default;

void WebSocketCloseImmediatelyHandler::OnHandshakeComplete() {
  connection()->StartClosingHandshake(/*code=*/std::nullopt, /*message=*/"");
}

}  // namespace net::test_server

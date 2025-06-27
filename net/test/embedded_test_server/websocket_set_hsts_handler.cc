// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/websocket_set_hsts_handler.h"

#include "base/memory/scoped_refptr.h"
#include "net/test/embedded_test_server/websocket_connection.h"
#include "net/test/embedded_test_server/websocket_echo_handler.h"
#include "net/test/embedded_test_server/websocket_handler.h"

namespace net::test_server {

WebSocketSetHstsHandler::WebSocketSetHstsHandler(
    scoped_refptr<WebSocketConnection> connection)
    : WebSocketHandler(std::move(connection)) {}

void WebSocketSetHstsHandler::OnHandshake(const HttpRequest& request) {
  CHECK(connection());
  connection()->SetResponseHeader("Strict-Transport-Security", "max-age=3600");
}

}  // namespace net::test_server

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_CREATE_WEBSOCKET_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_CREATE_WEBSOCKET_HANDLER_H_

#include <memory>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/websocket_handler.h"

namespace net::test_server {

class WebSocketHandler;
class WebSocketConnection;

using WebSocketHandlerCreator =
    base::RepeatingCallback<std::unique_ptr<WebSocketHandler>(
        scoped_refptr<WebSocketConnection> connection)>;

// Creates a handler that can be passed to
// EmbeddedTestServer::RegisterUpgradeRequestHandler() to implement a
// WebSocket protocol endpoint on `handle_path`, which should start with
// '/'. `websocket_handler_creator` is called for every valid incoming WebSocket
// handshake request on this path. It should create a subclass of
// WebSocketHandler and return it.
EmbeddedTestServer::HandleUpgradeRequestCallback CreateWebSocketHandler(
    std::string_view handle_path,
    WebSocketHandlerCreator websocket_handler_creator);

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_CREATE_WEBSOCKET_HANDLER_H_

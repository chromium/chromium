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
    WebSocketHandlerCreator websocket_handler_creator,
    EmbeddedTestServer* server);

// Registers a WebSocket handler for the specified subclass of WebSocketHandler.
// This template function streamlines registration by eliminating the need for
// a separate CreateHandler() method for each handler subclass. Instead, it
// binds the subclass directly to the embedded test server's upgrade request
// handler.
//
// Usage Example:
//   RegisterWebSocketHandler<MyWebSocketHandler>(embedded_test_server,
//   "/mypath");
// This registers `MyWebSocketHandler` with `embedded_test_server` so that a new
// instance is created for each WebSocket handshake on the specified path.
//
// Template Parameters:
//   - Handler: Subclass of WebSocketHandler defining the connection behavior.
//
// Parameters:
//   - embedded_test_server: The EmbeddedTestServer to register with.
//   - handle_path: Path where the handler responds to WebSocket requests
//   (starts with '/').
//
// Requirements:
//   - `Handler` must derive from `WebSocketHandler`.

template <typename Handler>
  requires std::is_base_of_v<WebSocketHandler, Handler>
void RegisterWebSocketHandler(EmbeddedTestServer* server,
                              std::string_view handle_path) {
  const auto websocket_handler_creator =
      base::BindRepeating([](scoped_refptr<WebSocketConnection> connection)
                              -> std::unique_ptr<WebSocketHandler> {
        return std::make_unique<Handler>(std::move(connection));
      });
  const auto callback =
      CreateWebSocketHandler(handle_path, websocket_handler_creator, server);

  server->RegisterUpgradeRequestHandler(callback);
}
}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_CREATE_WEBSOCKET_HANDLER_H_

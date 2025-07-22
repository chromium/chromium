// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_INSTALL_DEFAULT_WEBSOCKET_HANDLERS_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_INSTALL_DEFAULT_WEBSOCKET_HANDLERS_H_

#include <string_view>

#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace net::test_server {

// Installs default WebSocket handlers, such as the echo handler, on the given
// EmbeddedTestServer instance. HTTP handlers may be added directly to the
// passed in EmbeddedTestServer as well, to allow a single server to support
// both HTTP and WebSockets.
//
//
// Parameters:
// - `server`: The EmbeddedTestServer instance to configure.
//
// Note:
// - To ensure consistent behavior, it is recommended that only one file handler
//   (e.g., via `ServeFilesFromDirectory`) is installed per server instance.
// - If multiple calls to `ServeFilesFromDirectory` are necessary, ensure they
//   serve distinct sets of files or are added intentionally to the
//   request-handling chain.
void InstallDefaultWebSocketHandlers(EmbeddedTestServer* server);

// Converts a given HTTP or HTTPS URL to a corresponding WebSocket (ws) or
// Secure WebSocket (wss) URL, depending on the server's SSL configuration.
GURL ToWebSocketUrl(const GURL& url);

// Generates a WebSocket URL using the specified EmbeddedTestServer and a
// relative URL path, which must start with '/'. Returns a WebSocket URL
// prefixed with ws:// or wss:// based on the server's configuration.
GURL GetWebSocketURL(const EmbeddedTestServer& server,
                     std::string_view relative_url);

// Similar to the above GetWebSocketURL function but allows specifying a
// custom hostname in place of the default '127.0.0.1'. The hostname should
// resolve to 127.0.0.1 for local testing purposes.
GURL GetWebSocketURL(const EmbeddedTestServer& server,
                     std::string_view hostname,
                     std::string_view relative_url);

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_INSTALL_DEFAULT_WEBSOCKET_HANDLERS_H_

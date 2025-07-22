// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/install_default_websocket_handlers.h"

#include <string_view>

#include "base/check.h"
#include "net/test/embedded_test_server/create_websocket_handler.h"
#include "net/test/embedded_test_server/websocket_check_origin_handler.h"
#include "net/test/embedded_test_server/websocket_close_handler.h"
#include "net/test/embedded_test_server/websocket_close_immediately_handler.h"
#include "net/test/embedded_test_server/websocket_close_observer_handler.h"
#include "net/test/embedded_test_server/websocket_echo_handler.h"
#include "net/test/embedded_test_server/websocket_echo_request_headers_handler.h"
#include "net/test/embedded_test_server/websocket_set_hsts_handler.h"
#include "net/test/embedded_test_server/websocket_split_packet_close_handler.h"
#include "url/url_constants.h"

namespace net::test_server {

void InstallDefaultWebSocketHandlers(EmbeddedTestServer* server) {
  CHECK(server);

  // Register default WebSocket handlers.
  RegisterWebSocketHandler<WebSocketCheckOriginHandler>(server,
                                                        "/check-origin");
  RegisterWebSocketHandler<WebSocketCloseHandler>(server, "/close");
  RegisterWebSocketHandler<WebSocketCloseImmediatelyHandler>(
      server, "/close-immediately");
  RegisterWebSocketHandler<WebSocketCloseObserverHandler>(server,
                                                          "/close-observer");
  RegisterWebSocketHandler<WebSocketEchoHandler>(server,
                                                 "/echo-with-no-extension");
  RegisterWebSocketHandler<WebSocketEchoRequestHeadersHandler>(
      server, "/echo-request-headers");
  RegisterWebSocketHandler<WebSocketSplitPacketCloseHandler>(
      server, "/close-with-split-packet");
  RegisterWebSocketHandler<WebSocketSetHstsHandler>(server, "/set-hsts");
}

GURL ToWebSocketUrl(const GURL& url) {
  GURL::Replacements replacements;
  std::string_view websocket_scheme =
      (url.SchemeIs(url::kHttpsScheme) ? url::kWssScheme : url::kWsScheme);
  replacements.SetSchemeStr(websocket_scheme);
  return url.ReplaceComponents(replacements);
}

GURL GetWebSocketURL(const EmbeddedTestServer& server,
                     std::string_view relative_url) {
  DCHECK(server.Started()) << "Server must be started to get WebSocket URL";
  DCHECK(relative_url.starts_with("/")) << "Relative URL should start with '/'";

  GURL base_url = server.base_url().Resolve(relative_url);
  return ToWebSocketUrl(base_url);
}

GURL GetWebSocketURL(const EmbeddedTestServer& server,
                     std::string_view hostname,
                     std::string_view relative_url) {
  DCHECK(server.Started()) << "Server must be started to get WebSocket URL";
  DCHECK(relative_url.starts_with("/")) << "Relative URL should start with '/'";

  GURL local_url = GetWebSocketURL(server, relative_url);
  GURL::Replacements replacements;
  replacements.SetHostStr(hostname);
  return local_url.ReplaceComponents(replacements);
}

}  // namespace net::test_server

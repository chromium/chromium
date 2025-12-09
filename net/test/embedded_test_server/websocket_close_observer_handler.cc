// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/websocket_close_observer_handler.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/websocket_connection.h"

namespace net::test_server {

namespace {

// Global variables for managing connection state and close code. These values
// are shared across different instances of WebSocketCloseObserverHandler to
// enable coordination between "observer" and "observed" WebSocket roles.
constinit std::optional<uint16_t> g_code = std::nullopt;
constinit base::OnceClosure g_on_closed;

}  // namespace

WebSocketCloseObserverHandler::WebSocketCloseObserverHandler(
    scoped_refptr<WebSocketConnection> connection)
    : WebSocketHandler(std::move(connection)) {}

WebSocketCloseObserverHandler::~WebSocketCloseObserverHandler() = default;

void WebSocketCloseObserverHandler::SendBadRequest(std::string_view message) {
  const std::string response_content = base::StrCat({"Error: ", message});
  const std::string response =
      base::StrCat({"HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: ",
                    base::NumberToString(response_content.size()),
                    "\r\n"
                    "\r\n",
                    response_content});
  connection()->SendRaw(base::as_byte_span(response));
  connection()->DisconnectAfterAnyWritesDone();
}

void WebSocketCloseObserverHandler::OnHandshake(const HttpRequest& request) {
  CHECK(connection());

  std::string role;
  if (!GetValueForKeyInQuery(request.GetURL(), "role", &role)) {
    VLOG(1) << "Missing required 'role' parameter.";
    SendBadRequest("Missing required 'role' parameter.");
    return;
  }

  // Map the role string to the Role enum
  if (role == "observer") {
    role_ = Role::kObserver;
    BeObserver();
  } else if (role == "observed") {
    role_ = Role::kObserved;
  } else {
    VLOG(1) << "Invalid 'role' parameter: " << role;
    SendBadRequest("Invalid 'role' parameter.");
    return;
  }
}

void WebSocketCloseObserverHandler::OnClosingHandshake(
    std::optional<uint16_t> code,
    std::string_view message) {
  VLOG(3) << "OnClosingHandshake()";

  if (role_ == Role::kObserved) {
    g_code = code.value_or(1006);
    if (g_on_closed) {
      std::move(g_on_closed).Run();
    }
  }
}

void WebSocketCloseObserverHandler::BeObserver() {
  VLOG(3) << "BeObserver()";
  if (g_code) {
    SendCloseCode();
  } else {
    g_on_closed = base::BindOnce(&WebSocketCloseObserverHandler::SendCloseCode,
                                 base::Unretained(this));
  }
}

void WebSocketCloseObserverHandler::SendCloseCode() {
  CHECK(g_code);
  const std::string response =
      (*g_code == 1001) ? "OK" : "WRONG CODE " + base::NumberToString(*g_code);
  connection()->SendTextMessage(response);
}

}  // namespace net::test_server

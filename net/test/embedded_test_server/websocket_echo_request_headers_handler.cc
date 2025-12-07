// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/websocket_echo_request_headers_handler.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/test/embedded_test_server/websocket_connection.h"

namespace net::test_server {

WebSocketEchoRequestHeadersHandler::WebSocketEchoRequestHeadersHandler(
    scoped_refptr<WebSocketConnection> connection)
    : WebSocketHandler(std::move(connection)) {}

WebSocketEchoRequestHeadersHandler::~WebSocketEchoRequestHeadersHandler() =
    default;

void WebSocketEchoRequestHeadersHandler::OnHandshake(
    const HttpRequest& request) {
  CHECK(connection());

  base::Value::Dict headers_dict;

  // Convert headers to lowercase keys while retaining original values.
  for (const auto& header : request.headers) {
    headers_dict.Set(base::ToLowerASCII(header.first), header.second);
  }

  // Use base::WriteJson to serialize headers to JSON, assuming it will succeed.
  const std::string json_headers = base::WriteJson(headers_dict).value();
  connection()->SendTextMessage(json_headers);
}

}  // namespace net::test_server

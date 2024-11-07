// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/create_websocket_handler.h"

#include "base/base64.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "net/base/host_port_pair.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/websocket_connection.h"

namespace net::test_server {

namespace {

std::unique_ptr<HttpResponse> MakeErrorResponse(HttpStatusCode code,
                                                std::string_view content) {
  auto error_response = std::make_unique<BasicHttpResponse>();
  error_response->set_code(code);
  error_response->set_content(content);
  DVLOG(3) << "Error response created. Code: " << static_cast<int>(code)
           << ", Content: " << content;
  return error_response;
}

EmbeddedTestServer::UpgradeResultOrHttpResponse HandleWebSocketUpgrade(
    std::string_view handle_path,
    WebSocketHandlerCreator websocket_handler_creator,
    const HttpRequest& request,
    HttpConnection* connection) {
  DVLOG(3) << "Handling WebSocket upgrade for path: " << handle_path;

  if (request.relative_url != handle_path) {
    return UpgradeResult::kNotHandled;
  }

  if (request.method != METHOD_GET) {
    return base::unexpected(
        MakeErrorResponse(HttpStatusCode::HTTP_BAD_REQUEST,
                          "Invalid request method. Expected GET."));
  }

  // TODO(crbug.com/40812029): Check that the HTTP version is 1.1
  // See https://datatracker.ietf.org/doc/html/rfc6455#section-4.2.1

  auto host_header = request.headers.find("Host");
  if (host_header == request.headers.end()) {
    DVLOG(1) << "Host header is missing.";
    return base::unexpected(MakeErrorResponse(HttpStatusCode::HTTP_BAD_REQUEST,
                                              "Host header is missing."));
  }

  HostPortPair host_port = HostPortPair::FromString(host_header->second);
  if (!IsCanonicalizedHostCompliant(host_port.host())) {
    DVLOG(1) << "Host header is invalid: " << host_port.host();
    return base::unexpected(MakeErrorResponse(HttpStatusCode::HTTP_BAD_REQUEST,
                                              "Host header is invalid."));
  }

  auto upgrade_header = request.headers.find("Upgrade");
  if (upgrade_header == request.headers.end() ||
      !base::EqualsCaseInsensitiveASCII(upgrade_header->second, "websocket")) {
    DVLOG(1) << "Upgrade header is missing or invalid: "
             << upgrade_header->second;
    return base::unexpected(
        MakeErrorResponse(HttpStatusCode::HTTP_BAD_REQUEST,
                          "Upgrade header is missing or invalid."));
  }

  auto connection_header = request.headers.find("Connection");
  if (connection_header == request.headers.end()) {
    DVLOG(1) << "Connection header is missing.";
    return base::unexpected(MakeErrorResponse(HttpStatusCode::HTTP_BAD_REQUEST,
                                              "Connection header is missing."));
  }

  auto tokens =
      base::SplitStringPiece(connection_header->second, ",",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (!base::ranges::any_of(tokens, [](std::string_view token) {
        return base::EqualsCaseInsensitiveASCII(token, "Upgrade");
      })) {
    DVLOG(1) << "Connection header does not contain 'Upgrade'. Tokens: "
             << connection_header->second;
    return base::unexpected(
        MakeErrorResponse(HttpStatusCode::HTTP_BAD_REQUEST,
                          "Connection header does not contain 'Upgrade'."));
  }

  auto websocket_version_header = request.headers.find("Sec-WebSocket-Version");
  if (websocket_version_header == request.headers.end() ||
      websocket_version_header->second != "13") {
    DVLOG(1) << "Invalid or missing Sec-WebSocket-Version: "
             << websocket_version_header->second;
    return base::unexpected(MakeErrorResponse(
        HttpStatusCode::HTTP_BAD_REQUEST, "Sec-WebSocket-Version must be 13."));
  }

  auto sec_websocket_key_iter = request.headers.find("Sec-WebSocket-Key");
  if (sec_websocket_key_iter == request.headers.end()) {
    DVLOG(1) << "Sec-WebSocket-Key header is missing.";
    return base::unexpected(
        MakeErrorResponse(HttpStatusCode::HTTP_BAD_REQUEST,
                          "Sec-WebSocket-Key header is missing."));
  }

  auto decoded = base::Base64Decode(sec_websocket_key_iter->second);
  if (!decoded || decoded->size() != 16) {
    DVLOG(1) << "Sec-WebSocket-Key is invalid or has incorrect length.";
    return base::unexpected(MakeErrorResponse(
        HttpStatusCode::HTTP_BAD_REQUEST,
        "Sec-WebSocket-Key is invalid or has incorrect length."));
  }

  std::unique_ptr<StreamSocket> socket = connection->TakeSocket();
  CHECK(socket);

  auto websocket_connection = base::MakeRefCounted<WebSocketConnection>(
      std::move(socket), sec_websocket_key_iter->second);

  auto handler = websocket_handler_creator.Run(websocket_connection);
  handler->OnHandshake(request);
  websocket_connection->SetHandler(std::move(handler));
  websocket_connection->SendHandshakeResponse();
  return UpgradeResult::kUpgraded;
}

}  // namespace

EmbeddedTestServer::HandleUpgradeRequestCallback CreateWebSocketHandler(
    std::string_view handle_path,
    WebSocketHandlerCreator websocket_handler_creator) {
  return base::BindRepeating(&HandleWebSocketUpgrade, handle_path,
                             websocket_handler_creator);
}

}  // namespace net::test_server

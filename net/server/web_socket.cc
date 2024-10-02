// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/web_socket.h"

#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/hash/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "net/server/http_connection.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/server/web_socket_encoder.h"
#include "net/websockets/websocket_deflate_parameters.h"
#include "net/websockets/websocket_extension.h"
#include "net/websockets/websocket_handshake_constants.h"

namespace net {

namespace {

std::string ExtensionsHeaderString(
    const std::vector<WebSocketExtension>& extensions) {
  if (extensions.empty())
    return std::string();

  std::string result = "Sec-WebSocket-Extensions: " + extensions[0].ToString();
  for (size_t i = 1; i < extensions.size(); ++i)
    result += ", " + extensions[i].ToString();
  return result + "\r\n";
}

std::string ValidResponseString(
    const std::string& accept_hash,
    const std::vector<WebSocketExtension>& extensions) {
  return base::StringPrintf(
      "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: %s\r\n"
      "%s"
      "\r\n",
      accept_hash.c_str(), ExtensionsHeaderString(extensions).c_str());
}

}  // namespace

WebSocket::WebSocket(HttpServer* server, HttpConnection* connection)
    : server_(server), connection_(connection) {}

WebSocket::~WebSocket() = default;

void WebSocket::Accept(const HttpServerRequestInfo& request,
                       const NetworkTrafficAnnotationTag traffic_annotation) {
  std::string version = request.GetHeaderValue("sec-websocket-version");
  if (version != "8" && version != "13") {
    SendErrorResponse("Invalid request format. The version is not valid.",
                      traffic_annotation);
    return;
  }

  std::string key = request.GetHeaderValue("sec-websocket-key");
  if (key.empty()) {
    SendErrorResponse(
        "Invalid request format. Sec-WebSocket-Key is empty or isn't "
        "specified.",
        traffic_annotation);
    return;
  }
  std::string encoded_hash = base::Base64Encode(
      base::SHA1HashString(key + websockets::kWebSocketGuid));

  std::vector<WebSocketExtension> response_extensions;
  auto i = request.headers.find("sec-websocket-extensions");
  if (i == request.headers.end()) {
    encoder_ = WebSocketEncoder::CreateServer();
  } else {
    WebSocketDeflateParameters params;
    encoder_ = WebSocketEncoder::CreateServer(i->second, &params);
    if (!encoder_) {
      Fail();
      return;
    }
    if (encoder_->deflate_enabled()) {
      DCHECK(params.IsValidAsResponse());
      response_extensions.push_back(params.AsExtension());
    }
  }
  server_->SendRaw(connection_->id(),
                   ValidResponseString(encoded_hash, response_extensions),
                   traffic_annotation);
  traffic_annotation_ = std::make_unique<NetworkTrafficAnnotationTag>(
      NetworkTrafficAnnotationTag(traffic_annotation));
}

WebSocketParseResult WebSocket::Read(std::string* message) {
  if (closed_)
    return WebSocketParseResult::FRAME_CLOSE;

  if (!encoder_) {
    // RFC6455, section 4.1 says "Once the client's opening handshake has been
    // sent, the client MUST wait for a response from the server before sending
    // any further data". If |encoder_| is null here, ::Accept either has not
    // been called at all, or has rejected a request rather than producing
    // a server handshake. Either way, the client clearly couldn't have gotten
    // a proper server handshake, so error out, especially since this method
    // can't proceed without an |encoder_|.
    return WebSocketParseResult::FRAME_ERROR;
  }

  WebSocketParseResult result = WebSocketParseResult::FRAME_OK_MIDDLE;
  HttpConnection::ReadIOBuffer* read_buf = connection_->read_buf();
  std::string_view frame(read_buf->StartOfBuffer(), read_buf->GetSize());
  int bytes_consumed = 0;
  result = encoder_->DecodeFrame(frame, &bytes_consumed, message);
  read_buf->DidConsume(bytes_consumed);

  if (result == WebSocketParseResult::FRAME_CLOSE) {
    // The current websocket implementation does not initiate the Close
    // handshake before closing the connection.
    // Therefore the received Close frame most likely belongs to the client that
    // initiated the Closing handshake.
    // According to https://datatracker.ietf.org/doc/html/rfc6455#section-5.5.1
    // if an endpoint receives a Close frame and did not previously send a
    // Close frame, the endpoint MUST send a Close frame in response.
    // It also MAY provide the close reason listed in
    // https://datatracker.ietf.org/doc/html/rfc6455#section-7.4.1.
    // As the closure was initiated by the client the "normal closure" status
    // code is appropriate.
    std::string code = "\x03\xe8";  // code = 1000;
    std::string encoded;
    encoder_->EncodeCloseFrame(code, 0, &encoded);
    server_->SendRaw(connection_->id(), encoded, *traffic_annotation_);

    closed_ = true;
  }

  if (result == WebSocketParseResult::FRAME_PING) {
    if (!traffic_annotation_)
      return WebSocketParseResult::FRAME_ERROR;
    Send(*message, WebSocketFrameHeader::kOpCodePong, *traffic_annotation_);
  }
  return result;
}

void WebSocket::Send(std::string_view message,
                     WebSocketFrameHeader::OpCodeEnum op_code,
                     const NetworkTrafficAnnotationTag traffic_annotation) {
  if (closed_)
    return;
  std::string encoded;
  switch (op_code) {
    case WebSocketFrameHeader::kOpCodeText:
      encoder_->EncodeTextFrame(message, 0, &encoded);
      break;

    case WebSocketFrameHeader::kOpCodePong:
      encoder_->EncodePongFrame(message, 0, &encoded);
      break;

    default:
      // Only Pong and Text frame types are supported.
      NOTREACHED_IN_MIGRATION();
  }
  server_->SendRaw(connection_->id(), encoded, traffic_annotation);
}

void WebSocket::Fail() {
  closed_ = true;
  // TODO(yhirano): The server SHOULD log the problem.
  server_->Close(connection_->id());
}

void WebSocket::SendErrorResponse(
    const std::string& message,
    const NetworkTrafficAnnotationTag traffic_annotation) {
  if (closed_)
    return;
  closed_ = true;
  server_->Send500(connection_->id(), message, traffic_annotation);
}

}  // namespace net

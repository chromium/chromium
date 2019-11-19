// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_stream_base.h"

#include <unordered_set>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/websockets/websocket_extension.h"
#include "net/websockets/websocket_extension_parser.h"
#include "net/websockets/websocket_handshake_constants.h"

namespace net {

// static
std::string WebSocketHandshakeStreamBase::MultipleHeaderValuesMessage(
    const std::string& header_name) {
  return std::string("'") + header_name +
         "' header must not appear more than once in a response";
}

// static
void WebSocketHandshakeStreamBase::AddVectorHeaderIfNonEmpty(
    const char* name,
    const std::vector<std::string>& value,
    HttpRequestHeaders* headers) {
  if (value.empty())
    return;
  headers->SetHeader(name, base::JoinString(value, ", "));
}

// static
bool WebSocketHandshakeStreamBase::ValidateSubProtocol(
    const HttpResponseHeaders* headers,
    const std::vector<std::string>& requested_sub_protocols,
    std::string* sub_protocol,
    std::string* failure_message) {
  size_t iter = 0;
  std::string value;
  std::unordered_set<std::string> requested_set(requested_sub_protocols.begin(),
                                                requested_sub_protocols.end());
  int count = 0;
  bool has_multiple_protocols = false;
  bool has_invalid_protocol = false;

  while (!has_invalid_protocol || !has_multiple_protocols) {
    std::string temp_value;
    if (!headers->EnumerateHeader(&iter, websockets::kSecWebSocketProtocol,
                                  &temp_value))
      break;
    value = temp_value;
    if (requested_set.count(value) == 0)
      has_invalid_protocol = true;
    if (++count > 1)
      has_multiple_protocols = true;
  }

  if (has_multiple_protocols) {
    *failure_message =
        MultipleHeaderValuesMessage(websockets::kSecWebSocketProtocol);
    return false;
  } else if (count > 0 && requested_sub_protocols.size() == 0) {
    *failure_message = std::string(
                           "Response must not include 'Sec-WebSocket-Protocol' "
                           "header if not present in request: ") +
                       value;
    return false;
  } else if (has_invalid_protocol) {
    *failure_message = "'Sec-WebSocket-Protocol' header value '" + value +
                       "' in response does not match any of sent values";
    return false;
  } else if (requested_sub_protocols.size() > 0 && count == 0) {
    *failure_message =
        "Sent non-empty 'Sec-WebSocket-Protocol' header "
        "but no response was received";
    return false;
  }
  *sub_protocol = value;
  return true;
}

// static
bool WebSocketHandshakeStreamBase::ValidateExtensions(
    const HttpResponseHeaders* headers,
    std::string* accepted_extensions_descriptor,
    std::string* failure_message,
    WebSocketExtensionParams* params) {
  size_t iter = 0;
  std::string header_value;
  std::vector<std::string> header_values;
  // TODO(ricea): If adding support for additional extensions, generalise this
  // code.
  bool seen_permessage_deflate = false;
  while (headers->EnumerateHeader(&iter, websockets::kSecWebSocketExtensions,
                                  &header_value)) {
    WebSocketExtensionParser parser;
    if (!parser.Parse(header_value)) {
      // TODO(yhirano) Set appropriate failure message.
      *failure_message =
          "'Sec-WebSocket-Extensions' header value is "
          "rejected by the parser: " +
          header_value;
      return false;
    }

    const std::vector<WebSocketExtension>& extensions = parser.extensions();
    for (const auto& extension : extensions) {
      if (extension.name() == "permessage-deflate") {
        if (seen_permessage_deflate) {
          *failure_message = "Received duplicate permessage-deflate response";
          return false;
        }
        seen_permessage_deflate = true;
        auto& deflate_parameters = params->deflate_parameters;
        if (!deflate_parameters.Initialize(extension, failure_message) ||
            !deflate_parameters.IsValidAsResponse(failure_message)) {
          *failure_message = "Error in permessage-deflate: " + *failure_message;
          return false;
        }
        // Note that we don't have to check the request-response compatibility
        // here because we send a request compatible with any valid responses.
        // TODO(yhirano): Place a DCHECK here.

        header_values.push_back(header_value);
      } else {
        *failure_message = "Found an unsupported extension '" +
                           extension.name() +
                           "' in 'Sec-WebSocket-Extensions' header";
        return false;
      }
    }
  }
  *accepted_extensions_descriptor = base::JoinString(header_values, ", ");
  params->deflate_enabled = seen_permessage_deflate;
  return true;
}

void WebSocketHandshakeStreamBase::RecordHandshakeResult(
    HandshakeResult result) {
  UMA_HISTOGRAM_ENUMERATION("Net.WebSocket.HandshakeResult2", result,
                            HandshakeResult::NUM_HANDSHAKE_RESULT_TYPES);
}

}  // namespace net

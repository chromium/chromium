// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_stream_base.h"

#include <stddef.h>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/websockets/websocket_extension.h"
#include "net/websockets/websocket_extension_parser.h"
#include "net/websockets/websocket_handshake_constants.h"

namespace net {

namespace {

size_t AddVectorHeaderIfNonEmpty(const char* name,
                                 const std::vector<std::string>& value,
                                 HttpRequestHeaders* headers) {
  if (value.empty()) {
    return 0u;
  }
  std::string joined = base::JoinString(value, ", ");
  const size_t size = joined.size();
  headers->SetHeader(name, std::move(joined));
  return size;
}

}  // namespace

// static
std::string WebSocketHandshakeStreamBase::MultipleHeaderValuesMessage(
    const std::string& header_name) {
  return base::StrCat(
      {"'", header_name,
       "' header must not appear more than once in a response"});
}

// static
void WebSocketHandshakeStreamBase::AddVectorHeaders(
    const std::vector<std::string>& extensions,
    const std::vector<std::string>& protocols,
    HttpRequestHeaders* headers) {
  AddVectorHeaderIfNonEmpty(websockets::kSecWebSocketExtensions, extensions,
                            headers);
  const size_t protocol_header_size = AddVectorHeaderIfNonEmpty(
      websockets::kSecWebSocketProtocol, protocols, headers);
  base::UmaHistogramCounts10000("Net.WebSocket.ProtocolHeaderSize",
                                protocol_header_size);
}

// static
bool WebSocketHandshakeStreamBase::ValidateSubProtocol(
    const HttpResponseHeaders* headers,
    const std::vector<std::string>& requested_sub_protocols,
    std::string* sub_protocol,
    std::string* failure_message) {
  size_t iter = 0;
  std::optional<std::string> value;
  while (std::optional<std::string_view> maybe_value = headers->EnumerateHeader(
             &iter, websockets::kSecWebSocketProtocol)) {
    if (value) {
      *failure_message =
          MultipleHeaderValuesMessage(websockets::kSecWebSocketProtocol);
      return false;
    }
    if (requested_sub_protocols.empty()) {
      *failure_message =
          base::StrCat({"Response must not include 'Sec-WebSocket-Protocol' "
                        "header if not present in request: ",
                        *maybe_value});
      return false;
    }
    auto it = std::ranges::find(requested_sub_protocols, *maybe_value);
    if (it == requested_sub_protocols.end()) {
      *failure_message =
          base::StrCat({"'Sec-WebSocket-Protocol' header value '", *maybe_value,
                        "' in response does not match any of sent values"});
      return false;
    }
    value = *maybe_value;
  }

  if (!requested_sub_protocols.empty() && !value.has_value()) {
    *failure_message =
        "Sent non-empty 'Sec-WebSocket-Protocol' header "
        "but no response was received";
    return false;
  }
  if (value) {
    *sub_protocol = *value;
  } else {
    sub_protocol->clear();
  }
  return true;
}

// static
bool WebSocketHandshakeStreamBase::ValidateExtensions(
    const HttpResponseHeaders* headers,
    std::string* accepted_extensions_descriptor,
    std::string* failure_message,
    WebSocketExtensionParams* params) {
  size_t iter = 0;
  std::vector<std::string> header_values;
  // TODO(ricea): If adding support for additional extensions, generalise this
  // code.
  bool seen_permessage_deflate = false;
  while (std::optional<std::string_view> header_value =
             headers->EnumerateHeader(&iter,
                                      websockets::kSecWebSocketExtensions)) {
    WebSocketExtensionParser parser;
    if (!parser.Parse(*header_value)) {
      // TODO(yhirano) Set appropriate failure message.
      *failure_message =
          base::StrCat({"'Sec-WebSocket-Extensions' header value is "
                        "rejected by the parser: ",
                        *header_value});
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

        header_values.emplace_back(*header_value);
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

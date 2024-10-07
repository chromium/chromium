// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_EXTENSION_PARSER_H_
#define NET_WEBSOCKETS_WEBSOCKET_EXTENSION_PARSER_H_

#include <stddef.h>

#include <string>
#include <string_view>
#include <vector>

#include "net/base/net_export.h"
#include "net/websockets/websocket_extension.h"

namespace net {

class NET_EXPORT_PRIVATE WebSocketExtensionParser {
 public:
  WebSocketExtensionParser();

  WebSocketExtensionParser(const WebSocketExtensionParser&) = delete;
  WebSocketExtensionParser& operator=(const WebSocketExtensionParser&) = delete;

  ~WebSocketExtensionParser();

  // Parses the given string as a Sec-WebSocket-Extensions header value.
  //
  // There must be no newline characters in the input. LWS-concatenation must
  // have already been done before calling this method.
  //
  // Returns true if the method was successful (no syntax error was found).
  bool Parse(const char* data, size_t size);
  bool Parse(std::string_view data) { return Parse(data.data(), data.size()); }

  // Returns the result of the last Parse() method call.
  const std::vector<WebSocketExtension>& extensions() const {
    return extensions_;
  }

 private:
  [[nodiscard]] bool Consume(char c);
  [[nodiscard]] bool ConsumeExtension(WebSocketExtension* extension);
  [[nodiscard]] bool ConsumeExtensionParameter(
      WebSocketExtension::Parameter* parameter);
  [[nodiscard]] bool ConsumeToken(std::string_view* token);
  [[nodiscard]] bool ConsumeQuotedToken(std::string* token);
  void ConsumeSpaces();
  [[nodiscard]] bool Lookahead(char c);
  [[nodiscard]] bool ConsumeIfMatch(char c);

  // The current position in the input string.
  const char* current_;
  // The pointer of the end of the input string.
  const char* end_;
  std::vector<WebSocketExtension> extensions_;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_EXTENSION_PARSER_H_

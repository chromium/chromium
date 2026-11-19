// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_extension_parser.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "net/http/http_util.h"
#include "net/websockets/websocket_extension.h"

namespace net {
namespace {

class WebSocketExtensionParser {
 public:
  explicit WebSocketExtensionParser(std::string_view data) : remaining_(data) {}

  WebSocketExtensionParser(const WebSocketExtensionParser&) = delete;
  WebSocketExtensionParser& operator=(const WebSocketExtensionParser&) = delete;

  std::vector<WebSocketExtension> Parse() && {
    std::vector<WebSocketExtension> extensions;

    do {
      if (!ConsumeExtension(extensions)) {
        return {};
      }
    } while (ConsumeSpacesThenConsumeIfMatch(','));

    if (!remaining_.empty()) {
      return {};
    }
    return extensions;
  }

 private:
  [[nodiscard]] bool ConsumeExtension(
      std::vector<WebSocketExtension>& extensions) {
    std::optional<std::string_view> name = ConsumeToken();
    if (!name) {
      return false;
    }
    WebSocketExtension& extension = extensions.emplace_back(std::string(*name));

    while (ConsumeSpacesThenConsumeIfMatch(';')) {
      if (!ConsumeExtensionParameter(extension)) {
        return false;
      }
    }

    return true;
  }

  [[nodiscard]] bool ConsumeExtensionParameter(WebSocketExtension& extension) {
    std::optional<std::string_view> name = ConsumeToken();
    if (!name) {
      return false;
    }

    if (!ConsumeSpacesThenConsumeIfMatch('=')) {
      extension.Add(WebSocketExtension::Parameter(std::string(*name)));
      return true;
    }

    std::optional<std::string> value;
    if (ConsumeSpacesThenConsumeIfMatch('"')) {
      value = ConsumeRestOfQuotedToken();
    } else {
      value = ConsumeToken();
    }
    if (!value) {
      return false;
    }
    extension.Add(
        WebSocketExtension::Parameter(std::string(*name), *std::move(value)));
    return true;
  }

  [[nodiscard]] std::optional<std::string_view> ConsumeToken() {
    ConsumeSpaces();
    size_t count = 0;
    while (count < remaining_.size() &&
           HttpUtil::IsTokenChar(remaining_[count])) {
      ++count;
    }
    if (count == 0) {
      return std::nullopt;
    }
    std::string_view token = remaining_.substr(0, count);
    remaining_.remove_prefix(count);
    return token;
  }

  // Consumes the remainder of a quoted-string parameter value after the opening
  // '"' has been consumed. Per RFC 6455 Section 9.1, the unescaped value must
  // be a valid non-empty token.
  [[nodiscard]] std::optional<std::string> ConsumeRestOfQuotedToken() {
    std::string token;
    while (!remaining_.empty()) {
      char c = remaining_.front();
      remaining_.remove_prefix(1);
      if (c == '"') {
        if (token.empty()) {
          return std::nullopt;
        }
        return token;
      }
      if (c == '\\') {
        if (remaining_.empty()) {
          return std::nullopt;
        }
        c = remaining_.front();
        remaining_.remove_prefix(1);
      }
      if (!HttpUtil::IsTokenChar(c)) {
        return std::nullopt;
      }
      token += c;
    }
    return std::nullopt;
  }

  void ConsumeSpaces() {
    while (!remaining_.empty() && HttpUtil::IsLWS(remaining_.front())) {
      remaining_.remove_prefix(1);
    }
  }

  [[nodiscard]] bool ConsumeSpacesThenConsumeIfMatch(char c) {
    ConsumeSpaces();
    if (!remaining_.starts_with(c)) {
      return false;
    }
    remaining_.remove_prefix(1);
    return true;
  }

  // Unprocessed part of the input string.
  std::string_view remaining_;
};

}  // namespace

std::vector<WebSocketExtension> ParseWebSocketExtensions(
    std::string_view data) {
  return WebSocketExtensionParser(data).Parse();
}

}  // namespace net

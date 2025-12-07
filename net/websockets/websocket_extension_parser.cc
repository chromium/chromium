// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_extension_parser.h"

#include "base/check_op.h"
#include "net/http/http_util.h"

namespace net {
namespace {
class WebSocketExtensionParser {
 public:
  WebSocketExtensionParser() = default;
  ~WebSocketExtensionParser() = default;

  WebSocketExtensionParser(const WebSocketExtensionParser&) = delete;
  WebSocketExtensionParser& operator=(const WebSocketExtensionParser&) = delete;

  std::vector<WebSocketExtension> Parse(std::string_view data) {
    std::vector<WebSocketExtension> extensions;
    remaining_ = data;

    bool failed = false;
    do {
      WebSocketExtension extension;
      if (!ConsumeExtension(&extension)) {
        failed = true;
        break;
      }
      extensions.push_back(extension);

      ConsumeSpaces();
    } while (ConsumeIfMatch(','));

    if (!failed && remaining_.empty()) {
      return extensions;
    }

    return {};
  }

 private:
  [[nodiscard]] bool Consume(char c) {
    ConsumeSpaces();
    if (remaining_.empty() || c != remaining_.front()) {
      return false;
    }
    remaining_.remove_prefix(1);
    return true;
  }

  [[nodiscard]] bool ConsumeExtension(WebSocketExtension* extension) {
    std::string_view name;
    if (!ConsumeToken(&name)) {
      return false;
    }
    *extension = WebSocketExtension(std::string(name));

    while (ConsumeIfMatch(';')) {
      WebSocketExtension::Parameter parameter((std::string()));
      if (!ConsumeExtensionParameter(&parameter)) {
        return false;
      }
      extension->Add(parameter);
    }

    return true;
  }

  [[nodiscard]] bool ConsumeExtensionParameter(
      WebSocketExtension::Parameter* parameter) {
    std::string_view name;
    if (!ConsumeToken(&name)) {
      return false;
    }

    if (!ConsumeIfMatch('=')) {
      *parameter = WebSocketExtension::Parameter(std::string(name));
      return true;
    }

    std::string value_string;
    if (Lookahead('\"')) {
      if (!ConsumeQuotedToken(&value_string)) {
        return false;
      }
    } else {
      std::string_view value;
      if (!ConsumeToken(&value)) {
        return false;
      }
      value_string = std::string(value);
    }
    *parameter = WebSocketExtension::Parameter(std::string(name), value_string);
    return true;
  }

  [[nodiscard]] bool ConsumeToken(std::string_view* token) {
    ConsumeSpaces();
    size_t count = 0;
    while (count < remaining_.size() &&
           HttpUtil::IsTokenChar(remaining_[count])) {
      ++count;
    }
    if (count == 0) {
      return false;
    }
    *token = remaining_.substr(0, count);
    remaining_.remove_prefix(count);
    return true;
  }

  [[nodiscard]] bool ConsumeQuotedToken(std::string* token) {
    if (!Consume('"')) {
      return false;
    }

    *token = "";
    while (!remaining_.empty() && remaining_.front() != '"') {
      if (remaining_.front() == '\\') {
        remaining_.remove_prefix(1);
        if (remaining_.empty()) {
          return false;
        }
      }
      if (!HttpUtil::IsTokenChar(remaining_.front())) {
        return false;
      }
      *token += remaining_.front();
      remaining_.remove_prefix(1);
    }
    if (remaining_.empty()) {
      return false;
    }
    DCHECK_EQ(remaining_.front(), '"');

    remaining_.remove_prefix(1);

    return !token->empty();
  }

  void ConsumeSpaces() {
    while (!remaining_.empty() &&
           (remaining_.front() == ' ' || remaining_.front() == '\t')) {
      remaining_.remove_prefix(1);
    }
  }

  [[nodiscard]] bool Lookahead(char c) {
    std::string_view saved = remaining_;
    bool result = Consume(c);
    remaining_ = saved;
    return result;
  }

  [[nodiscard]] bool ConsumeIfMatch(char c) {
    std::string_view saved = remaining_;
    if (!Consume(c)) {
      remaining_ = saved;
      return false;
    }
    return true;
  }

  // Unprocessed part of the input string.
  std::string_view remaining_;
};

}  // namespace

std::vector<WebSocketExtension> ParseWebSocketExtensions(
    std::string_view data) {
  WebSocketExtensionParser parser;
  return parser.Parse(data);
}

}  // namespace net

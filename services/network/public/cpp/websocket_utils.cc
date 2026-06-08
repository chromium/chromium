// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/websocket_utils.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <string_view>

#include "net/base/isolation_info.h"
#include "url/gurl.h"

namespace network {

namespace {

bool IsValidSubprotocolCharacter(char character) {
  constexpr auto kMinimumProtocolCharacter = '!';  // U+0021.
  constexpr auto kMaximumProtocolCharacter = '~';  // U+007E.
  // Set to true if character does not matches "separators" ABNF defined in
  // RFC2616. SP and HT are excluded since the range check excludes them.
  const bool is_separator =
      character == '"' || character == '(' || character == ')' ||
      character == ',' || character == '/' ||
      (character >= ':' &&
       character <=
           '@')  // U+003A - U+0040 (':', ';', '<', '=', '>', '?', '@').
      || (character >= '[' &&
          character <= ']')  // U+005B - U+005D ('[', '\\', ']').
      || character == '{' || character == '}';
  return character >= kMinimumProtocolCharacter &&
         character <= kMaximumProtocolCharacter && !is_separator;
}

bool IsValidSubprotocolString(const std::string& protocol) {
  if (protocol.empty()) {
    return false;
  }
  return std::ranges::all_of(protocol, IsValidSubprotocolCharacter);
}

bool IsValidProtocols(const std::vector<std::string>& requested_protocols) {
  // Fail if not all elements in |protocols| are valid.
  if (!std::ranges::all_of(requested_protocols, IsValidSubprotocolString)) {
    return false;
  }

  // Fail if there're duplicated elements in |protocols|.
  std::vector<std::string_view> protocols(std::from_range, requested_protocols);
  std::ranges::sort(protocols);
  if (std::ranges::adjacent_find(protocols) != protocols.end()) {
    return false;
  }

  return true;
}

}  // namespace

std::optional<std::string> VerifyWebSocketConnectParameters(
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const net::IsolationInfo& isolation_info) {
  if (isolation_info.request_type() !=
      net::IsolationInfo::RequestType::kOther) {
    return "WebSocket's IsolationInfo::RequestType must be kOther";
  }

  if (!url.SchemeIsWSOrWSS()) {
    return "Invalid scheme.";
  }

  if (!IsValidProtocols(requested_protocols)) {
    return "Invalid protocols.";
  }

  return std::nullopt;
}

}  // namespace network

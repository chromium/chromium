// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/avail_language_header_parser.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "net/http/structured_headers.h"

namespace network {

std::optional<std::vector<std::string>> ParseAvailLanguage(
    const std::string& header) {
  // Avail-Language is a sh-list of tokens to header; see:
  // https://mnot.github.io/I-D/draft-nottingham-http-availability-hints.html#section-5.3
  std::optional<net::structured_headers::List> maybe_list =
      net::structured_headers::ParseList(base::ToLowerASCII(header));

  if (!maybe_list.has_value()) {
    return std::nullopt;
  }

  for (const auto& list_item : maybe_list.value()) {
    // Make sure not a nested list.
    if (list_item.member.size() != 1u) {
      return std::nullopt;
    }
    if (!list_item.member[0].item.is_token()) {
      return std::nullopt;
    }
  }

  std::vector<std::string> result;
  std::vector<std::string> non_default_languages;
  for (const auto& list_item : maybe_list.value()) {
    const std::string& token_value = list_item.member[0].item.GetString();
    // If the language is default like `en;d`, insert the language `en` into the
    // beginning of the list. As the current parsing parameter rule, a list
    // parameter `d` equals `d=?1`. See `ReadParameters()` in
    // net/third_party/quiche/src/quiche/common/structured_headers.cc
    if (list_item.params.size() == 1 && list_item.params[0].first == "d" &&
        list_item.params[0].second.is_boolean() &&
        list_item.params[0].second.GetBoolean()) {
      result.push_back(token_value);
    } else {
      non_default_languages.push_back(token_value);
    }
  }
  result.insert(result.end(), non_default_languages.begin(),
                non_default_languages.end());
  return std::make_optional(std::move(result));
}

}  // namespace network

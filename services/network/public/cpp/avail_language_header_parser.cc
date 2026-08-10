// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/avail_language_header_parser.h"

#include <algorithm>
#include <iterator>
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
    if (list_item.member_is_inner_list || list_item.member.size() != 1 ||
        !list_item.member.front().item.is_token()) {
      return std::nullopt;
    }
  }

  std::vector<std::string> result;
  result.reserve(maybe_list->size());

  std::vector<std::string> non_default_languages;

  for (auto& list_item : maybe_list.value()) {
    // Dereferencing this is safe due to the `is_token` check and early return
    // above.
    std::string* token_value = list_item.member.front().item.GetIfToken();
    // If the language is default like `en;d`, insert the language `en` into the
    // beginning of the list.
    const bool* is_default =
        (list_item.params.size() == 1 && list_item.params.front().first == "d")
            ? list_item.params.front().second.GetIfBoolean()
            : nullptr;

    if (is_default && *is_default) {
      result.emplace_back(std::move(*token_value));
    } else {
      non_default_languages.emplace_back(std::move(*token_value));
    }
  }

  std::ranges::move(non_default_languages, std::back_inserter(result));
  return result;
}

}  // namespace network

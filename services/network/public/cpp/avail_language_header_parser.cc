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

#include "net/http/structured_headers.h"

namespace network {

std::optional<std::vector<std::string>> ParseAvailLanguage(
    const std::string& header) {
  // https://projects.mnot.net/I-D/draft-nottingham-http-availability-hints.html#name-content-language
  std::optional<net::structured_headers::List> maybe_list =
      net::structured_headers::ParseList(header);

  if (!maybe_list.has_value()) {
    return std::nullopt;
  }

  for (const auto& list_item : maybe_list.value()) {
    const auto item_and_params = list_item.GetWithParamsIfItem();
    // Make sure not a nested list.
    if (!item_and_params.has_value() || !item_and_params->first.is_token()) {
      return std::nullopt;
    }
  }

  std::vector<std::string> result;
  result.reserve(maybe_list->size());

  std::vector<std::string> non_default_languages;

  for (auto& list_item : maybe_list.value()) {
    // Dereferencing these is safe due to the `is_token` check and early return
    // above.
    auto [item, params] = *list_item.GetWithParamsIfItem();
    std::string* token_value = item.GetIfToken();
    // If the language is default like `en;d`, insert the language `en` into the
    // beginning of the list.
    const bool* is_default = (params.size() == 1 && params.front().first == "d")
                                 ? params.front().second.GetIfBoolean()
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

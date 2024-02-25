// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/network/public/cpp/content_language_parser.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "net/http/structured_headers.h"

namespace network {

std::optional<std::vector<std::string>> ParseContentLanguages(
    const std::string& header) {
  // Content-Language is a sh-dictionary of tokens to header; see:
  // https://httpwg.org/specs/rfc7231.html#rfc.section.3.1.3.2.
  std::optional<net::structured_headers::List> maybe_list =
      net::structured_headers::ParseList(base::ToLowerASCII(header));
  if (!maybe_list.has_value())
    return std::nullopt;

  for (const auto& list_item : maybe_list.value()) {
    // Make sure not a nested list.
    if (list_item.member.size() != 1u)
      return std::nullopt;
    if (!list_item.member[0].item.is_token())
      return std::nullopt;
  }

  std::vector<std::string> result;
  for (const auto& list_item : maybe_list.value()) {
    const std::string& token_value = list_item.member[0].item.GetString();
    result.push_back(token_value);
  }
  return std::make_optional(std::move(result));
}

}  // namespace network
// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/supports_loading_mode/supports_loading_mode_parser.h"

#include <optional>
#include <ranges>

#include "base/ranges/algorithm.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/supports_loading_mode.mojom.h"

namespace network {

namespace {

constexpr std::string_view kSupportsLoadingMode = "Supports-Loading-Mode";
constexpr struct KnownLoadingMode {
  std::string_view token;
  mojom::LoadingMode enumerator;
} kKnownLoadingModes[] = {
    {"default", mojom::LoadingMode::kDefault},
    {"uncredentialed-prefetch", mojom::LoadingMode::kUncredentialedPrefetch},
    {"uncredentialed-prerender", mojom::LoadingMode::kUncredentialedPrerender},
    {"credentialed-prerender", mojom::LoadingMode::kCredentialedPrerender},
    {"fenced-frame", mojom::LoadingMode::kFencedFrame},
};

}  // namespace

mojom::SupportsLoadingModePtr ParseSupportsLoadingMode(
    std::string_view header_value) {
  // A parse error in the HTTP structured headers syntax is a parse error for
  // the header value as a whole.
  auto list = net::structured_headers::ParseList(header_value);
  if (!list)
    return nullptr;

  // The default loading mode is assumed to be supported.
  std::vector<mojom::LoadingMode> modes{mojom::LoadingMode::kDefault};

  for (const net::structured_headers::ParameterizedMember& member : *list) {
    // No supported mode currently is specified as an inner list or takes
    // parameters.
    if (member.member_is_inner_list || !member.params.empty())
      continue;

    // All supported modes are tokens.
    const net::structured_headers::ParameterizedItem& item = member.member[0];
    DCHECK(item.params.empty());
    if (!item.item.is_token())
      continue;

    // Each supported token maps 1:1 to an enumerator.
    const auto& token = item.item.GetString();
    const auto* it =
        base::ranges::find(kKnownLoadingModes, token, &KnownLoadingMode::token);
    if (it == std::ranges::end(kKnownLoadingModes)) {
      continue;
    }

    modes.push_back(it->enumerator);
  }

  // Order and repetition are not significant.
  // Canonicalize by making the vector sorted and unique.
  base::ranges::sort(modes);
  modes.erase(base::ranges::unique(modes), modes.end());
  return mojom::SupportsLoadingMode::New(std::move(modes));
}

mojom::SupportsLoadingModePtr ParseSupportsLoadingMode(
    const net::HttpResponseHeaders& headers) {
  std::string header_value;
  if (!headers.GetNormalizedHeader(kSupportsLoadingMode, &header_value))
    return nullptr;
  return ParseSupportsLoadingMode(header_value);
}

}  // namespace network

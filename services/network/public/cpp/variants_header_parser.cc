// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/variants_header_parser.h"

#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "net/http/structured_headers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {
// Add Support Variants for existing content negotiation mechanisms, See
// https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-variants-06#appendix-A.
// For reduce accept-language fingerprinting, we only care about accept-language
// header.
SupportedVariantsNameSet MakeSupportedVariantsNameSet() {
  return {"accept-language"};
}

const SupportedVariantsNameSet& GetSupportedVariantsNameSet() {
  static const base::NoDestructor<SupportedVariantsNameSet> set(
      MakeSupportedVariantsNameSet());
  return *set;
}

absl::optional<std::vector<mojom::VariantsHeaderPtr>> ParseVariantsHeaders(
    const std::string& header) {
  // Variants is a sh-dictionary of tokens to header; see:
  // https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-variants-06#section-2
  absl::optional<net::structured_headers::Dictionary> maybe_dictionary =
      net::structured_headers::ParseDictionary(base::ToLowerASCII(header));
  if (!maybe_dictionary.has_value())
    return absl::nullopt;

  std::vector<mojom::VariantsHeaderPtr> parsed_headers;

  // Now filter only supported variants.
  const SupportedVariantsNameSet& supported_set = GetSupportedVariantsNameSet();
  for (const auto& dictionary_pair : maybe_dictionary.value()) {
    if (!supported_set.count(dictionary_pair.first))
      continue;

    std::vector<std::string> parsed_values;
    for (const auto& member : dictionary_pair.second.member) {
      if (!member.item.is_token())
        continue;
      parsed_values.push_back(member.item.GetString());
    }
    auto parsed = mojom::VariantsHeader::New(dictionary_pair.first,
                                             std::move(parsed_values));
    parsed_headers.push_back(std::move(parsed));
  }
  return absl::make_optional(std::move(parsed_headers));
}

}  // namespace network
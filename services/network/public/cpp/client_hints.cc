// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/client_hints.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

const char kPrefersColorSchemeDark[] = "dark";
const char kPrefersColorSchemeLight[] = "light";

const char kPrefersReducedMotionNoPreference[] = "no-preference";
const char kPrefersReducedMotionReduce[] = "reduce";

const char kPrefersReducedTransparencyNoPreference[] = "no-preference";
const char kPrefersReducedTransparencyReduce[] = "reduce";

namespace {

struct ClientHintNameComparator {
  constexpr bool operator()(std::string_view lhs, std::string_view rhs) const {
    return base::CompareCaseInsensitiveASCII(lhs, rhs) < 0;
  }
};

consteval auto MakeDecodeMap() {
  std::pair<std::string_view, network::mojom::WebClientHintsType>
      data[kClientHintToNameMap.size()] = {};
  size_t i = 0;
  for (const auto& [type, header] : kClientHintToNameMap) {
    data[i++] = {header, type};
  }
  return base::MakeFixedFlatMap<std::string_view,
                                network::mojom::WebClientHintsType>(
      std::move(data), ClientHintNameComparator());
}

constexpr auto kDecodeMap = MakeDecodeMap();

}  // namespace

std::optional<std::vector<network::mojom::WebClientHintsType>>
ParseClientHintsHeader(const std::string& header) {
  // Accept-CH is an sh-list of tokens; see:
  // https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-header-structure-19#section-3.1
  std::optional<net::structured_headers::List> maybe_list =
      net::structured_headers::ParseList(header);
  if (!maybe_list.has_value())
    return std::nullopt;

  // Standard validation rules: we want a list of tokens, so this better
  // only have tokens (but params are OK!)
  for (const auto& list_item : maybe_list.value()) {
    const net::structured_headers::ParameterizedItem* item =
        list_item.GetIfItem();
    // Make sure not a nested list.
    if (!item || !item->item.is_token()) {
      return std::nullopt;
    }
  }

  std::vector<network::mojom::WebClientHintsType> result;

  // Now convert those to actual hint enums.
  for (const auto& list_item : maybe_list.value()) {
    const std::string& token_value = list_item.GetIfItem()->item.GetToken();
    auto iter = kDecodeMap.find(token_value);
    if (iter != kDecodeMap.end()) {
      result.push_back(iter->second);
    }
  }  // for list_item
  return result;
}

ClientHintToDelegatedThirdPartiesHeader::
    ClientHintToDelegatedThirdPartiesHeader() = default;

ClientHintToDelegatedThirdPartiesHeader::
    ~ClientHintToDelegatedThirdPartiesHeader() = default;

ClientHintToDelegatedThirdPartiesHeader::
    ClientHintToDelegatedThirdPartiesHeader(
        const ClientHintToDelegatedThirdPartiesHeader&) = default;

const ClientHintToDelegatedThirdPartiesHeader
ParseClientHintToDelegatedThirdPartiesHeader(const std::string& header,
                                             MetaCHType type) {
  switch (type) {
    case MetaCHType::HttpEquivAcceptCH: {
      // ParseClientHintsHeader should have been called instead.
      NOTREACHED();
    }
    case MetaCHType::HttpEquivDelegateCH: {
      // We're building a scoped down version of
      // ParsingContext::ParseFeaturePolicyToIR that supports only client hint
      // features.
      ClientHintToDelegatedThirdPartiesHeader result;
      const auto& entries =
          base::SplitString(base::ToLowerASCII(header), ";",
                            base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      for (const auto& entry : entries) {
        const auto& components = base::SplitString(
            entry, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
        // Shouldn't be possible given this only processes non-empty parts.
        DCHECK(components.size());
        auto found_token = kDecodeMap.find(components[0]);
        // Bail early if the token is invalid
        if (found_token == kDecodeMap.end()) {
          continue;
        }
        std::vector<url::Origin> delegates;
        for (size_t i = 1; i < components.size(); ++i) {
          const GURL gurl = GURL(components[i]);
          if (!gurl.is_valid()) {
            result.had_invalid_origins = true;
            continue;
          }
          url::Origin origin = url::Origin::Create(gurl);
          if (origin.opaque()) {
            result.had_invalid_origins = true;
            continue;
          }
          delegates.push_back(origin);
        }
        result.map.insert(std::make_pair(found_token->second, delegates));
      }
      return result;
    }
  }
}

// static
void LogClientHintsPersistenceMetrics(
    const base::TimeTicks& persistence_started,
    std::size_t hints_stored) {
  base::UmaHistogramTimes("ClientHints.StoreLatency",
                          base::TimeTicks::Now() - persistence_started);
  base::UmaHistogramExactLinear("ClientHints.UpdateEventCount", 1, 2);
  base::UmaHistogramCounts100("ClientHints.UpdateSize", hints_stored);
}

}  // namespace network

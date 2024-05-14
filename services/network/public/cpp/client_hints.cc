// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/client_hints.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
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

const char* const kWebEffectiveConnectionTypeMapping[] = {
    "4g" /* Unknown */, "4g" /* Offline */, "slow-2g" /* Slow 2G */,
    "2g" /* 2G */,      "3g" /* 3G */,      "4g" /* 4G */
};

const size_t kWebEffectiveConnectionTypeMappingCount =
    std::size(kWebEffectiveConnectionTypeMapping);

ClientHintToNameMap MakeClientHintToNameMap() {
  return {
      {network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
       "device-memory"},
      {network::mojom::WebClientHintsType::kDpr_DEPRECATED, "dpr"},
      {network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED, "width"},
      {network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED,
       "viewport-width"},
      {network::mojom::WebClientHintsType::kRtt_DEPRECATED, "rtt"},
      {network::mojom::WebClientHintsType::kDownlink_DEPRECATED, "downlink"},
      {network::mojom::WebClientHintsType::kEct_DEPRECATED, "ect"},
      {network::mojom::WebClientHintsType::kUA, "sec-ch-ua"},
      {network::mojom::WebClientHintsType::kUAArch, "sec-ch-ua-arch"},
      {network::mojom::WebClientHintsType::kUAPlatform, "sec-ch-ua-platform"},
      {network::mojom::WebClientHintsType::kUAModel, "sec-ch-ua-model"},
      {network::mojom::WebClientHintsType::kUAMobile, "sec-ch-ua-mobile"},
      {network::mojom::WebClientHintsType::kUAFullVersion,
       "sec-ch-ua-full-version"},
      {network::mojom::WebClientHintsType::kUAPlatformVersion,
       "sec-ch-ua-platform-version"},
      {network::mojom::WebClientHintsType::kPrefersColorScheme,
       "sec-ch-prefers-color-scheme"},
      {network::mojom::WebClientHintsType::kUABitness, "sec-ch-ua-bitness"},
      {network::mojom::WebClientHintsType::kViewportHeight,
       "sec-ch-viewport-height"},
      {network::mojom::WebClientHintsType::kDeviceMemory,
       "sec-ch-device-memory"},
      {network::mojom::WebClientHintsType::kDpr, "sec-ch-dpr"},
      {network::mojom::WebClientHintsType::kResourceWidth, "sec-ch-width"},
      {network::mojom::WebClientHintsType::kViewportWidth,
       "sec-ch-viewport-width"},
      {network::mojom::WebClientHintsType::kUAFullVersionList,
       "sec-ch-ua-full-version-list"},
      {network::mojom::WebClientHintsType::kUAWoW64, "sec-ch-ua-wow64"},
      {network::mojom::WebClientHintsType::kSaveData, "save-data"},
      {network::mojom::WebClientHintsType::kPrefersReducedMotion,
       "sec-ch-prefers-reduced-motion"},
      {network::mojom::WebClientHintsType::kUAFormFactors,
       "sec-ch-ua-form-factors"},
      {network::mojom::WebClientHintsType::kPrefersReducedTransparency,
       "sec-ch-prefers-reduced-transparency"},
  };
}

const ClientHintToNameMap& GetClientHintToNameMap() {
  static const base::NoDestructor<ClientHintToNameMap> map(
      MakeClientHintToNameMap());
  return *map;
}

namespace {

struct ClientHintNameComparator {
  bool operator()(const std::string& lhs, const std::string& rhs) const {
    return base::CompareCaseInsensitiveASCII(lhs, rhs) < 0;
  }
};

using DecodeMap = base::flat_map<std::string,
                                 network::mojom::WebClientHintsType,
                                 ClientHintNameComparator>;

DecodeMap MakeDecodeMap() {
  DecodeMap result;
  for (const auto& elem : network::GetClientHintToNameMap()) {
    const auto& type = elem.first;
    const auto& header = elem.second;
    result.insert(std::make_pair(header, type));
  }
  return result;
}

const DecodeMap& GetDecodeMap() {
  static const base::NoDestructor<DecodeMap> decode_map(MakeDecodeMap());
  return *decode_map;
}

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
    // Make sure not a nested list.
    if (list_item.member.size() != 1u)
      return std::nullopt;
    if (!list_item.member[0].item.is_token())
      return std::nullopt;
  }

  std::vector<network::mojom::WebClientHintsType> result;

  // Now convert those to actual hint enums.
  const DecodeMap& decode_map = GetDecodeMap();
  for (const auto& list_item : maybe_list.value()) {
    const std::string& token_value = list_item.member[0].item.GetString();
    auto iter = decode_map.find(token_value);
    if (iter != decode_map.end())
      result.push_back(iter->second);
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
  const DecodeMap& decode_map = GetDecodeMap();

  switch (type) {
    case MetaCHType::HttpEquivAcceptCH: {
      // ParseClientHintsHeader should have been called instead.
      NOTREACHED_IN_MIGRATION();
      return ClientHintToDelegatedThirdPartiesHeader();
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
        auto found_token = decode_map.find(components[0]);
        // Bail early if the token is invalid
        if (found_token == decode_map.end())
          continue;
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

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/client_hints.h"

#include <utility>
#include <vector>

#include "base/cxx17_backports.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "net/http/structured_headers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

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
      {network::mojom::WebClientHintsType::kUAReduced, "sec-ch-ua-reduced"},
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
      {network::mojom::WebClientHintsType::kFullUserAgent, "sec-ch-ua-full"},
      {network::mojom::WebClientHintsType::kUAWoW64, "sec-ch-ua-wow64"},
      {network::mojom::WebClientHintsType::kSaveData, "save-data"},
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

absl::optional<std::vector<network::mojom::WebClientHintsType>>
ParseClientHintsHeader(const std::string& header) {
  // Accept-CH is an sh-list of tokens; see:
  // https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-header-structure-19#section-3.1
  absl::optional<net::structured_headers::List> maybe_list =
      net::structured_headers::ParseList(header);
  if (!maybe_list.has_value())
    return absl::nullopt;

  // Standard validation rules: we want a list of tokens, so this better
  // only have tokens (but params are OK!)
  for (const auto& list_item : maybe_list.value()) {
    // Make sure not a nested list.
    if (list_item.member.size() != 1u)
      return absl::nullopt;
    if (!list_item.member[0].item.is_token())
      return absl::nullopt;
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
  return absl::make_optional(std::move(result));
}

ClientHintToDelegatedThirdPartiesHeader::
    ClientHintToDelegatedThirdPartiesHeader() = default;

ClientHintToDelegatedThirdPartiesHeader::
    ~ClientHintToDelegatedThirdPartiesHeader() = default;

ClientHintToDelegatedThirdPartiesHeader::
    ClientHintToDelegatedThirdPartiesHeader(
        const ClientHintToDelegatedThirdPartiesHeader&) = default;

absl::optional<const ClientHintToDelegatedThirdPartiesHeader>
ParseClientHintToDelegatedThirdPartiesHeader(const std::string& header) {
  // Accept-CH is an sh-dictionary of tokens to origins; see:
  // https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-header-structure-19#section-3.2
  absl::optional<net::structured_headers::Dictionary> maybe_dictionary =
      // We need to lower-case the string here or dictionary parsing refuses to
      // see the keys.
      net::structured_headers::ParseDictionary(base::ToLowerASCII(header));
  if (!maybe_dictionary.has_value())
    return absl::nullopt;

  ClientHintToDelegatedThirdPartiesHeader result;

  // Now convert those to actual hint enums.
  const DecodeMap& decode_map = GetDecodeMap();
  for (const auto& dictionary_pair : maybe_dictionary.value()) {
    std::vector<url::Origin> delegates;
    for (const auto& member : dictionary_pair.second.member) {
      if (!member.item.is_token())
        continue;
      const GURL maybe_gurl = GURL(member.item.GetString());
      if (!maybe_gurl.is_valid()) {
        result.had_invalid_origins = true;
        continue;
      }
      url::Origin maybe_origin = url::Origin::Create(maybe_gurl);
      if (maybe_origin.opaque()) {
        result.had_invalid_origins = true;
        continue;
      }
      delegates.push_back(maybe_origin);
    }
    const std::string& client_hint_string = dictionary_pair.first;
    auto iter = decode_map.find(client_hint_string);
    if (iter != decode_map.end())
      result.map.insert(std::make_pair(iter->second, delegates));
  }  // for dictionary_pair
  return absl::make_optional(std::move(result));
}

}  // namespace network

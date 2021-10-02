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
  };
}

const ClientHintToNameMap& GetClientHintToNameMap() {
  static const base::NoDestructor<ClientHintToNameMap> map(
      MakeClientHintToNameMap());
  return *map;
}

namespace {

struct ClientHintNameCompator {
  bool operator()(const std::string& lhs, const std::string& rhs) const {
    return base::CompareCaseInsensitiveASCII(lhs, rhs) < 0;
  }
};

using DecodeMap = base::flat_map<std::string,
                                 network::mojom::WebClientHintsType,
                                 ClientHintNameCompator>;

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
  // https://httpwg.org/http-extensions/client-hints.html#rfc.section.3.1
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

base::TimeDelta ParseAcceptCHLifetime(const std::string& header) {
  int64_t persist_duration_seconds = 0;
  if (!base::StringToInt64(header, &persist_duration_seconds) ||
      persist_duration_seconds <= 0)
    return base::TimeDelta();

  return base::Seconds(persist_duration_seconds);
}

}  // namespace network

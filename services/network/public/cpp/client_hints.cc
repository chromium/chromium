// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/client_hints.h"

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "net/http/structured_headers.h"

namespace network {

const char* const kClientHintsNameMapping[] = {"device-memory",
                                               "dpr",
                                               "width",
                                               "viewport-width",
                                               "rtt",
                                               "downlink",
                                               "ect",
                                               "lang",
                                               "sec-ch-ua",
                                               "sec-ch-ua-arch",
                                               "sec-ch-ua-platform",
                                               "sec-ch-ua-model",
                                               "sec-ch-ua-mobile",
                                               "sec-ch-ua-full-version",
                                               "sec-ch-ua-platform-version"};

const size_t kClientHintsMappingsCount = base::size(kClientHintsNameMapping);

static_assert(
    base::size(kClientHintsNameMapping) ==
        (static_cast<int>(network::mojom::WebClientHintsType::kMaxValue) + 1),
    "Client Hint name table size must match network::mojom::WebClientHintsType "
    "range");

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
  for (size_t i = 0;
       i < static_cast<int>(network::mojom::WebClientHintsType::kMaxValue) + 1;
       ++i) {
    result.insert(
        std::make_pair(kClientHintsNameMapping[i],
                       static_cast<network::mojom::WebClientHintsType>(i)));
  }
  return result;
}

const DecodeMap& GetDecodeMap() {
  static const base::NoDestructor<DecodeMap> decode_map(MakeDecodeMap());
  return *decode_map;
}

}  // namespace

base::Optional<std::vector<network::mojom::WebClientHintsType>>
ParseClientHintsHeader(const std::string& header) {
  // Accept-CH is an sh-list of tokens; see:
  // https://httpwg.org/http-extensions/client-hints.html#rfc.section.3.1
  base::Optional<net::structured_headers::List> maybe_list =
      net::structured_headers::ParseList(header);
  if (!maybe_list.has_value())
    return base::nullopt;

  // Standard validation rules: we want a list of tokens, so this better
  // only have tokens (but params are OK!)
  for (const auto& list_item : maybe_list.value()) {
    // Make sure not a nested list.
    if (list_item.member.size() != 1u)
      return base::nullopt;
    if (!list_item.member[0].item.is_token())
      return base::nullopt;
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
  return base::make_optional(std::move(result));
}

base::TimeDelta ParseAcceptCHLifetime(const std::string& header) {
  int64_t persist_duration_seconds = 0;
  if (!base::StringToInt64(header, &persist_duration_seconds) ||
      persist_duration_seconds <= 0)
    return base::TimeDelta();

  return base::TimeDelta::FromSeconds(persist_duration_seconds);
}

}  // namespace network

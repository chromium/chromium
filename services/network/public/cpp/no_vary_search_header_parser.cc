// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/no_vary_search_header_parser.h"

#include "net/http/http_no_vary_search_data.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"

namespace network {
namespace {
mojom::NoVarySearchParseError ConvertParseError(
    const net::HttpNoVarySearchData::ParseErrorEnum& parse_error) {
  switch (parse_error) {
    case net::HttpNoVarySearchData::ParseErrorEnum::kOk:
      return mojom::NoVarySearchParseError::kOk;
    case net::HttpNoVarySearchData::ParseErrorEnum::kDefaultValue:
      return mojom::NoVarySearchParseError::kDefaultValue;
    case net::HttpNoVarySearchData::ParseErrorEnum::kNotDictionary:
      return mojom::NoVarySearchParseError::kNotDictionary;
    case net::HttpNoVarySearchData::ParseErrorEnum::kUnknownDictionaryKey:
      return mojom::NoVarySearchParseError::kUnknownDictionaryKey;
    case net::HttpNoVarySearchData::ParseErrorEnum::kNonBooleanKeyOrder:
      return mojom::NoVarySearchParseError::kNonBooleanKeyOrder;
    case net::HttpNoVarySearchData::ParseErrorEnum::kParamsNotStringList:
      return mojom::NoVarySearchParseError::kParamsNotStringList;
    case net::HttpNoVarySearchData::ParseErrorEnum::kExceptNotStringList:
      return mojom::NoVarySearchParseError::kExceptNotStringList;
    case net::HttpNoVarySearchData::ParseErrorEnum::kExceptWithoutTrueParams:
      return mojom::NoVarySearchParseError::kExceptWithoutTrueParams;
  }
}
}  // namespace

mojom::NoVarySearchWithParseErrorPtr ParseNoVarySearch(
    const net::HttpResponseHeaders& headers) {
  // See No-Vary-Search header structure at
  // https://github.com/WICG/nav-speculation/blob/main/no-vary-search.md#the-header
  const auto no_vary_search_data =
      net::HttpNoVarySearchData::ParseFromHeaders(headers);
  if (!no_vary_search_data.has_value()) {
    return mojom::NoVarySearchWithParseError::NewParseError(
        ConvertParseError(no_vary_search_data.error()));
  }

  mojom::NoVarySearchPtr no_vary_search = network::mojom::NoVarySearch::New();
  no_vary_search->vary_on_key_order = no_vary_search_data->vary_on_key_order();
  if (no_vary_search_data->vary_by_default()) {
    no_vary_search->search_variance =
        mojom::SearchParamsVariance::NewNoVaryParams(std::vector<std::string>(
            no_vary_search_data->no_vary_params().begin(),
            no_vary_search_data->no_vary_params().end()));
    return mojom::NoVarySearchWithParseError::NewNoVarySearch(
        std::move(no_vary_search));
  }
  no_vary_search->search_variance = mojom::SearchParamsVariance::NewVaryParams(
      std::vector<std::string>(no_vary_search_data->vary_params().begin(),
                               no_vary_search_data->vary_params().end()));
  return mojom::NoVarySearchWithParseError::NewNoVarySearch(
      std::move(no_vary_search));
}

}  // namespace network

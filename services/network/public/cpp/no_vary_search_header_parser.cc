// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/no_vary_search_header_parser.h"

#include "base/strings/stringprintf.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {
namespace {
const char kNoVarySearchSpecProposalUrl[] =
    "https://wicg.github.io/nav-speculation/no-vary-search.html";

const char kRFC8941DictionaryDefinitionUrl[] =
    "https://www.rfc-editor.org/rfc/rfc8941.html#name-dictionaries";

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

absl::optional<std::string> GetNoVarySearchConsoleMessage(
    const mojom::NoVarySearchParseError& error,
    const GURL& preloaded_url) {
  switch (error) {
    case network::mojom::NoVarySearchParseError::kOk:
      return absl::nullopt;
    case network::mojom::NoVarySearchParseError::kDefaultValue:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " is equivalent to the default search variance. No-Vary-Search"
          " header can be safely removed. See No-Vary-Search specification for"
          " more information: %s.",
          preloaded_url.spec().c_str(), kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kNotDictionary:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " is not a dictionary as defined in RFC8941: %s. The header"
          " will be ignored. Please fix this error. See No-Vary-Search"
          " specification for more information: %s.",
          preloaded_url.spec().c_str(), kRFC8941DictionaryDefinitionUrl,
          kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kUnknownDictionaryKey:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains unknown dictionary keys. Valid dictionary keys are:"
          " \"params\", \"except\", \"key-order\". The header will be"
          " ignored. Please fix this error. See No-Vary-Search"
          " specification for more information: %s.",
          preloaded_url.spec().c_str(), kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kNonBooleanKeyOrder:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains a \"key-order\" dictionary value that is not a boolean."
          " The header will be ignored. Please fix this error."
          " See No-Vary-Search specification for more information: %s.",
          preloaded_url.spec().c_str(), kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kParamsNotStringList:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains a \"params\" dictionary value that is not a list of"
          " strings or a boolean. The header will be ignored. Please fix this"
          " error."
          " See No-Vary-Search specification for more information: %s.",
          preloaded_url.spec().c_str(), kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kExceptNotStringList:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains an \"except\" dictionary value that is not a list of"
          " strings. The header will be ignored. Please fix this error."
          " See No-Vary-Search specification for more information: %s.",
          preloaded_url.spec().c_str(), kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kExceptWithoutTrueParams:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains an \"except\" dictionary key, without the \"params\""
          " dictionary key being set to true. The header will be ignored."
          " Please fix this error. See No-Vary-Search specification for"
          " more information: %s.",
          preloaded_url.spec().c_str(), kNoVarySearchSpecProposalUrl);
  }
}

absl::optional<std::string> GetNoVarySearchHintConsoleMessage(
    const mojom::NoVarySearchParseError& error) {
  switch (error) {
    case network::mojom::NoVarySearchParseError::kOk:
      return absl::nullopt;
    case network::mojom::NoVarySearchParseError::kDefaultValue:
      return base::StringPrintf(
          "No-Vary-Search hint value is equivalent to the default search"
          " variance. No-Vary-Search hint can be safely removed. See"
          " No-Vary-Search specification for more information: %s.",
          kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kNotDictionary:
      return base::StringPrintf(
          "No-Vary-Search hint value is not a dictionary as defined in"
          " RFC8941: %s. Please fix this error. See No-Vary-Search"
          " specification for more information: %s.",
          kRFC8941DictionaryDefinitionUrl, kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kUnknownDictionaryKey:
      return base::StringPrintf(
          "No-Vary-Search hint value contains unknown dictionary keys."
          " Valid dictionary keys are: \"params\", \"except\", \"key-order\"."
          " Please fix this error. See No-Vary-Search specification for more"
          " information: %s.",
          kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kNonBooleanKeyOrder:
      return base::StringPrintf(
          "No-Vary-Search hint value contains a \"key-order\" dictionary"
          " value that is not a boolean. Please fix this error."
          " See No-Vary-Search specification for more information: %s.",
          kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kParamsNotStringList:
      return base::StringPrintf(
          "No-Vary-Search hint value contains a \"params\" dictionary value"
          " that is not a list of strings or a boolean. Please fix this error."
          " See No-Vary-Search specification for more information: %s.",
          kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kExceptNotStringList:
      return base::StringPrintf(
          "No-Vary-Search hint value contains an \"except\" dictionary value"
          " that is not a list of strings. Please fix this error."
          " See No-Vary-Search specification for more information: %s.",
          kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kExceptWithoutTrueParams:
      return base::StringPrintf(
          "No-Vary-Search hint value contains an \"except\" dictionary key"
          " without the \"params\" dictionary key being set to true."
          " Please fix this error. See No-Vary-Search specification for"
          " more information: %s.",
          kNoVarySearchSpecProposalUrl);
  }
}

}  // namespace network

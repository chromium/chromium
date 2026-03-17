// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/no_vary_search_header_parser.h"

#include <optional>

#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"

namespace network {

namespace {

using mojom::NoVarySearchParseError;
using ParseErrorEnum = net::HttpNoVarySearchData::ParseErrorEnum;

const char kNoVarySearchSpecProposalUrl[] =
    "https://wicg.github.io/nav-speculation/no-vary-search.html";

const char kRFC8941DictionaryDefinitionUrl[] =
    "https://www.rfc-editor.org/rfc/rfc8941.html#name-dictionaries";

NoVarySearchParseError MapNoVarySearchError(ParseErrorEnum error) {
  switch (error) {
    case ParseErrorEnum::kOk:
      return NoVarySearchParseError::kOk;
    case ParseErrorEnum::kDefaultValue:
      return NoVarySearchParseError::kDefaultValue;
    case ParseErrorEnum::kNotDictionary:
      return NoVarySearchParseError::kNotDictionary;
    case ParseErrorEnum::kUnknownDictionaryKey:
      return NoVarySearchParseError::kUnknownDictionaryKey;
    case ParseErrorEnum::kNonBooleanKeyOrder:
      return NoVarySearchParseError::kNonBooleanKeyOrder;
    case ParseErrorEnum::kParamsNotStringList:
      return NoVarySearchParseError::kParamsNotStringList;
    case ParseErrorEnum::kExceptNotStringList:
      return NoVarySearchParseError::kExceptNotStringList;
    case ParseErrorEnum::kExceptWithoutTrueParams:
      return NoVarySearchParseError::kExceptWithoutTrueParams;
  }
  NOTREACHED();
}

mojom::NoVarySearchWithParseErrorPtr ToMojom(
    base::expected<net::HttpNoVarySearchData,
                   net::HttpNoVarySearchData::ParseErrorEnum>
        maybe_no_vary_search_data) {
  using Variance = mojom::SearchParamsVariance;

  if (!maybe_no_vary_search_data.has_value()) {
    return mojom::NoVarySearchWithParseError::NewParseError(
        MapNoVarySearchError(maybe_no_vary_search_data.error()));
  }

  const auto& no_vary_search_data = maybe_no_vary_search_data.value();
  mojom::NoVarySearchPtr no_vary_search = network::mojom::NoVarySearch::New();
  no_vary_search->vary_on_key_order = no_vary_search_data.vary_on_key_order();
  auto affected_params = no_vary_search_data.GetAffectedParams();
  no_vary_search->search_variance =
      no_vary_search_data.vary_by_default()
          ? Variance::NewNoVaryParams(std::move(affected_params))
          : Variance::NewVaryParams(std::move(affected_params));
  return mojom::NoVarySearchWithParseError::NewNoVarySearch(
      std::move(no_vary_search));
}

}  // namespace

mojom::NoVarySearchWithParseErrorPtr ParseNoVarySearchHeaderValue(
    std::string_view header_value) {
  return ToMojom(net::HttpNoVarySearchData::ParseFromHeaderValue(header_value));
}

mojom::NoVarySearchWithParseErrorPtr ParseNoVarySearch(
    const net::HttpResponseHeaders& headers) {
  return ToMojom(net::HttpNoVarySearchData::ParseFromHeaders(headers));
}

std::optional<std::string> GetNoVarySearchConsoleMessage(
    const mojom::NoVarySearchParseError& error,
    const GURL& preloaded_url) {
  switch (error) {
    case network::mojom::NoVarySearchParseError::kOk:
      return std::nullopt;
    case network::mojom::NoVarySearchParseError::kDefaultValue:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " is equivalent to the default behavior. No-Vary-Search"
          " header can be safely removed. See No-Vary-Search specification for"
          " more information: %s.",
          preloaded_url.spec().c_str(), kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kNotDictionary:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " is not a dictionary as defined in RFC8941: %s. The header"
          " will be ignored. See No-Vary-Search specification for more"
          " information: %s.",
          preloaded_url.spec().c_str(), kRFC8941DictionaryDefinitionUrl,
          kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kUnknownDictionaryKey:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains unknown dictionary keys. Valid dictionary keys are:"
          " \"params\", \"except\", \"key-order\". The header will be"
          " ignored. See No-Vary-Search specification for more"
          " information: %s.",
          preloaded_url.spec().c_str(), kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kNonBooleanKeyOrder:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains a \"key-order\" dictionary value that is not a boolean."
          " The header will be ignored."
          " See No-Vary-Search specification for more information: %s.",
          preloaded_url.spec().c_str(), kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kParamsNotStringList:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains a \"params\" dictionary value that is not a list of"
          " strings or a boolean. The header will be ignored."
          " See No-Vary-Search specification for more information: %s.",
          preloaded_url.spec().c_str(), kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kExceptNotStringList:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains an \"except\" dictionary value that is not a list of"
          " strings. The header will be ignored."
          " See No-Vary-Search specification for more information: %s.",
          preloaded_url.spec().c_str(), kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kExceptWithoutTrueParams:
      return base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains an \"except\" dictionary key, without the \"params\""
          " dictionary key being set to true. The header will be ignored."
          " See No-Vary-Search specification for more information: %s.",
          preloaded_url.spec().c_str(), kNoVarySearchSpecProposalUrl);
  }
}

std::optional<std::string> GetNoVarySearchHintConsoleMessage(
    const mojom::NoVarySearchParseError& error) {
  switch (error) {
    case network::mojom::NoVarySearchParseError::kOk:
      return std::nullopt;
    case network::mojom::NoVarySearchParseError::kDefaultValue:
      return base::StringPrintf(
          "No-Vary-Search hint value is equivalent to the default behavior."
          " No-Vary-Search hint can be safely removed. See"
          " No-Vary-Search specification for more information: %s.",
          kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kNotDictionary:
      return base::StringPrintf(
          "No-Vary-Search hint value is not a dictionary as defined in"
          " RFC8941: %s. See No-Vary-Search specification for more"
          " information: %s.",
          kRFC8941DictionaryDefinitionUrl, kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kUnknownDictionaryKey:
      return base::StringPrintf(
          "No-Vary-Search hint value contains unknown dictionary keys."
          " Valid dictionary keys are: \"params\", \"except\", \"key-order\"."
          " See No-Vary-Search specification for more information: %s.",
          kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kNonBooleanKeyOrder:
      return base::StringPrintf(
          "No-Vary-Search hint value contains a \"key-order\" dictionary"
          " value that is not a boolean."
          " See No-Vary-Search specification for more information: %s.",
          kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kParamsNotStringList:
      return base::StringPrintf(
          "No-Vary-Search hint value contains a \"params\" dictionary value"
          " that is not a list of strings or a boolean."
          " See No-Vary-Search specification for more information: %s.",
          kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kExceptNotStringList:
      return base::StringPrintf(
          "No-Vary-Search hint value contains an \"except\" dictionary value"
          " that is not a list of strings."
          " See No-Vary-Search specification for more information: %s.",
          kNoVarySearchSpecProposalUrl);
    case network::mojom::NoVarySearchParseError::kExceptWithoutTrueParams:
      return base::StringPrintf(
          "No-Vary-Search hint value contains an \"except\" dictionary key"
          " without the \"params\" dictionary key being set to true."
          " See No-Vary-Search specification for more information: %s.",
          kNoVarySearchSpecProposalUrl);
  }
}

}  // namespace network

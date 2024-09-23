// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/no_vary_search_header_parser.h"

#include <optional>

#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"

namespace network {
namespace {
const char kNoVarySearchSpecProposalUrl[] =
    "https://wicg.github.io/nav-speculation/no-vary-search.html";

const char kRFC8941DictionaryDefinitionUrl[] =
    "https://www.rfc-editor.org/rfc/rfc8941.html#name-dictionaries";
}  // namespace

mojom::NoVarySearchWithParseErrorPtr ParseNoVarySearch(
    const net::HttpResponseHeaders& headers) {
  // See No-Vary-Search header structure at
  // https://github.com/WICG/nav-speculation/blob/main/no-vary-search.md#the-header
  using Input = net::HttpNoVarySearchData::ParseErrorEnum;
  ASSIGN_OR_RETURN(const auto no_vary_search_data,
                   net::HttpNoVarySearchData::ParseFromHeaders(headers),
                   ([](Input error) {
                     const auto map_error = [](Input error) {
                       using Output = mojom::NoVarySearchParseError;
                       switch (error) {
                         case Input::kOk:
                           return Output::kOk;
                         case Input::kDefaultValue:
                           return Output::kDefaultValue;
                         case Input::kNotDictionary:
                           return Output::kNotDictionary;
                         case Input::kUnknownDictionaryKey:
                           return Output::kUnknownDictionaryKey;
                         case Input::kNonBooleanKeyOrder:
                           return Output::kNonBooleanKeyOrder;
                         case Input::kParamsNotStringList:
                           return Output::kParamsNotStringList;
                         case Input::kExceptNotStringList:
                           return Output::kExceptNotStringList;
                         case Input::kExceptWithoutTrueParams:
                           return Output::kExceptWithoutTrueParams;
                       }
                       NOTREACHED();
                     };
                     return mojom::NoVarySearchWithParseError::NewParseError(
                         map_error(error));
                   }));

  mojom::NoVarySearchPtr no_vary_search = network::mojom::NoVarySearch::New();
  no_vary_search->vary_on_key_order = no_vary_search_data.vary_on_key_order();
  if (no_vary_search_data.vary_by_default()) {
    no_vary_search->search_variance =
        mojom::SearchParamsVariance::NewNoVaryParams(std::vector<std::string>(
            no_vary_search_data.no_vary_params().begin(),
            no_vary_search_data.no_vary_params().end()));
    return mojom::NoVarySearchWithParseError::NewNoVarySearch(
        std::move(no_vary_search));
  }
  no_vary_search->search_variance = mojom::SearchParamsVariance::NewVaryParams(
      std::vector<std::string>(no_vary_search_data.vary_params().begin(),
                               no_vary_search_data.vary_params().end()));
  return mojom::NoVarySearchWithParseError::NewNoVarySearch(
      std::move(no_vary_search));
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

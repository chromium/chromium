// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/no_vary_search_helper.h"

#include <map>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {
// Parse No-Vary-Search from mojom structure received from network service.
net::HttpNoVarySearchData ParseHttpNoVarySearchDataFromMojom(
    const network::mojom::NoVarySearchPtr& no_vary_search_ptr) {
  if (no_vary_search_ptr->search_variance->is_vary_params()) {
    return net::HttpNoVarySearchData::CreateFromVaryParams(
        no_vary_search_ptr->search_variance->get_vary_params(),
        no_vary_search_ptr->vary_on_key_order);
  }
  return net::HttpNoVarySearchData::CreateFromNoVaryParams(
      no_vary_search_ptr->search_variance->get_no_vary_params(),
      no_vary_search_ptr->vary_on_key_order);
}
}  // namespace

NoVarySearchHelper::NoVarySearchHelper() = default;
NoVarySearchHelper::~NoVarySearchHelper() = default;

void NoVarySearchHelper::AddUrl(const GURL& url,
                                const network::mojom::URLResponseHead& head) {
  DCHECK(
      base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch));
  // Check if the prefetched response has a No-Vary-Search header and
  // add the prefetched response to |prefetches_with_no_vary_search_| if it
  // does.
  if (!(head.parsed_headers &&
        head.parsed_headers->no_vary_search_with_parse_error)) {
    return;
  }
  if (head.parsed_headers->no_vary_search_with_parse_error->is_parse_error()) {
    return;
  }
  auto no_vary_search_data = ParseHttpNoVarySearchDataFromMojom(
      head.parsed_headers->no_vary_search_with_parse_error
          ->get_no_vary_search());
  // Key the map using the url without the reference and query.
  // In almost all cases we expect one key to only one (url,no-vary-search)
  // mapping.
  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  prefetches_with_no_vary_search_[url.ReplaceComponents(replacements)]
      .emplace_back(url, std::move(no_vary_search_data));
}

void NoVarySearchHelper::MaybeSendErrorsToConsole(
    const GURL& url,
    const network::mojom::URLResponseHead& head,
    RenderFrameHost& rfh) const {
  DCHECK(
      base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch));
  if (!(head.parsed_headers &&
        head.parsed_headers->no_vary_search_with_parse_error)) {
    return;
  }
  if (!head.parsed_headers->no_vary_search_with_parse_error->is_parse_error()) {
    return;
  }

  blink::mojom::ConsoleMessageLevel error_level =
      blink::mojom::ConsoleMessageLevel::kError;
  std::string error_message;
  const std::string no_vary_search_spec =
      "https://wicg.github.io/nav-speculation/no-vary-search.html";
  switch (
      head.parsed_headers->no_vary_search_with_parse_error->get_parse_error()) {
    case network::mojom::NoVarySearchParseError::kOk:
      // In case everything is ok do nothing.
      return;
    case network::mojom::NoVarySearchParseError::kDefaultValue:
      error_level = blink::mojom::ConsoleMessageLevel::kWarning;
      error_message = base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " is equivalent to the default search variance. No-Vary-Search"
          " header can be safely removed. See No-Vary-Search specification for"
          " more information: %s.",
          url.spec().c_str(), no_vary_search_spec.c_str());
      break;
    case network::mojom::NoVarySearchParseError::kNotDictionary:
      error_message = base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " is not a dictionary as defined in RFC8941: %s. The header"
          " will be ignored. Please fix this error. See No-Vary-Search"
          " specification for more information: %s.",
          url.spec().c_str(),
          "https://www.rfc-editor.org/rfc/rfc8941.html#name-dictionaries",
          no_vary_search_spec.c_str());
      break;
    case network::mojom::NoVarySearchParseError::kUnknownDictionaryKey:
      error_message = base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains unknown dictionary keys. Valid dictionary keys are:"
          " \"params\", \"except\", \"key-order\". The header will be"
          " ignored. Please fix this error. See No-Vary-Search"
          " specification for more information: %s.",
          url.spec().c_str(), no_vary_search_spec.c_str());
      break;
    case network::mojom::NoVarySearchParseError::kNonBooleanKeyOrder:
      error_message = base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains a \"key-order\" dictionary value that is not a boolean."
          " The header will be ignored. Please fix this error."
          " See No-Vary-Search specification for more information: %s.",
          url.spec().c_str(), no_vary_search_spec.c_str());
      break;
    case network::mojom::NoVarySearchParseError::kParamsNotStringList:
      error_message = base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains a \"params\" dictionary value that is not a list of"
          " strings. The header will be ignored. Please fix this error."
          " See No-Vary-Search specification for more information: %s.",
          url.spec().c_str(), no_vary_search_spec.c_str());
      break;
    case network::mojom::NoVarySearchParseError::kExceptNotStringList:
      error_message = base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains an \"except\" dictionary value that is not a list of"
          " strings. The header will be ignored. Please fix this error."
          " See No-Vary-Search specification for more information: %s.",
          url.spec().c_str(), no_vary_search_spec.c_str());
      break;
    case network::mojom::NoVarySearchParseError::kExceptWithoutTrueParams:
      error_message = base::StringPrintf(
          "No-Vary-Search header value received for prefetched url %s"
          " contains an \"except\" dictionary key, without the \"params\""
          " dictionary key being set to true. The header will be ignored."
          " Please fix this error. See No-Vary-Search specification for"
          " more information: %s.",
          url.spec().c_str(), no_vary_search_spec.c_str());
      break;
  }
  rfh.AddMessageToConsole(error_level, error_message);
}

absl::optional<GURL> NoVarySearchHelper::MatchUrl(const GURL& url) const {
  DCHECK(
      base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch));
  // Check if we have a URL that matches No-Vary-Search
  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  const auto nvs_prefetch_iter =
      prefetches_with_no_vary_search_.find(url.ReplaceComponents(replacements));
  if (nvs_prefetch_iter == prefetches_with_no_vary_search_.end()) {
    return absl::nullopt;
  }
  const auto nvs_iter = std::find_if(
      nvs_prefetch_iter->second.begin(), nvs_prefetch_iter->second.end(),
      [&](const std::pair<GURL, net::HttpNoVarySearchData>& nvs) {
        return nvs.second.AreEquivalent(url, nvs.first);
      });
  if (nvs_iter == nvs_prefetch_iter->second.end()) {
    return absl::nullopt;
  }
  return nvs_iter->first;
}

const std::vector<std::pair<GURL, net::HttpNoVarySearchData>>*
NoVarySearchHelper::GetAllForUrlWithoutRefAndQueryForTesting(
    const GURL& url) const {
  DCHECK(
      base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch));
  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  const auto it =
      prefetches_with_no_vary_search_.find(url.ReplaceComponents(replacements));
  if (it == prefetches_with_no_vary_search_.end()) {
    return nullptr;
  }
  return &it->second;
}

}  // namespace content

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/no_vary_search_helper.h"

#include <map>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  if (head.parsed_headers && head.parsed_headers->no_vary_search) {
    auto no_vary_search_data =
        ParseHttpNoVarySearchDataFromMojom(head.parsed_headers->no_vary_search);
    // Key the map using the url without the reference and query.
    // In almost all cases we expect one key to only one (url,no-vary-search)
    // mapping.
    GURL::Replacements replacements;
    replacements.ClearRef();
    replacements.ClearQuery();
    prefetches_with_no_vary_search_[url.ReplaceComponents(replacements)]
        .emplace_back(url, std::move(no_vary_search_data));
  }
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

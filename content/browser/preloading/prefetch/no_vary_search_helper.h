// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_NO_VARY_SEARCH_HELPER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_NO_VARY_SEARCH_HELPER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_key.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"
#include "url/gurl.h"

namespace network::mojom {
class URLResponseHead;
}  // namespace network::mojom

namespace content {

class RenderFrameHost;

// Helpers to keep track of prefetched URLs that have No-Vary-Search
// header present in their responses.
// The source of truth is the `prefetches` and the helpers iterates over
// `prefetches` to find matching `PrefetchContainer`s.
namespace no_vary_search {

enum class MatchType {
  // URL is exactly the same.
  kExact,

  // URL is equivalent due to the received No-Vary-Search header.
  kNoVarySearchHeader,

  // URL is equivalent due to the No-Vary-Search hint.
  kNoVarySearchHint,

  // The non-ref/query parts of URL are the same.
  kOther
};

// Indicates whether `IterateCandidates` should continue or finish after
// `callback` is called.
enum class IterateCandidateResult { kContinue, kFinish };

// Call `callback` on every `PrefetchContainer`s that can match with `key`, in
// the order of
// 1. Exact match (`MatchType::kExact`).
// 2. No-Vary-Search matches (`MatchType::kNoVarySearch`), or
//    URLs with the same non-ref/query part as `key` (`MatchType::kOther`).
template <typename Value>
void IterateCandidates(
    const PrefetchKey& key,
    const std::map<PrefetchKey, Value>& prefetches,
    base::RepeatingCallback<IterateCandidateResult(const Value&, MatchType)>
        callback) {
  auto it_exact_match = prefetches.find(key);
  if (it_exact_match != prefetches.end() && it_exact_match->second) {
    if (callback.Run(it_exact_match->second, MatchType::kExact) ==
        IterateCandidateResult::kFinish) {
      return;
    }
  }

  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  GURL url_with_no_query = key.url().ReplaceComponents(replacements);

  // `std::map<GURL, ...>` is sorted by lexicographical string order of
  // the normalized URLs (`GURL::spec_`, i.e. `possibly_invalid_spec()`).
  // For a URL like `https://example.com/index.html?query#ref`, the
  // `lower_bound` call will get the first URL starting with
  // `https://example.com/index.html` (if any), and iterating by `++it` will get
  // the URLs starting with `https://example.com/index.html` in lexicographical
  // order until the URL without the `https://example.com/index.html` prefix is
  // encountered.
  //
  // This is possible because URLs with the same prefix are in consecutively
  // placed in the `std::map<PrefetchKey, ...>` iteration order.
  for (auto it = prefetches.lower_bound(key.WithNewUrl(url_with_no_query));
       it != prefetches.end(); ++it) {
    const GURL& prefetch_container_url = it->first.url();

    if (!key.NonUrlPartIsSame(it->first) ||
        !prefetch_container_url.possibly_invalid_spec().starts_with(
            url_with_no_query.possibly_invalid_spec())) {
      break;
    }

    // `it_exact_match` is already visited above and thus skipped.
    if (it == it_exact_match) {
      continue;
    }

    if (!it->second) {
      continue;
    }

    // The URLs starting with `https://example.com/index.html` don't necessarily
    // have the same non-ref/query parts. See
    // `NoVarySearchHelperTest.DoNotPrefixMatch` unit tests for concrete
    // examples.
    if (prefetch_container_url.ReplaceComponents(replacements) !=
        url_with_no_query) {
      continue;
    }

    const MatchType match_type = [&]() {
      const auto& prefetch_container = it->second;
      if (prefetch_container->IsNoVarySearchHeaderMatch(key.url())) {
        return MatchType::kNoVarySearchHeader;
      } else if (prefetch_container->ShouldWaitForNoVarySearchHeader(
                     key.url())) {
        return MatchType::kNoVarySearchHint;
      } else {
        return MatchType::kOther;
      }
    }();

    if (callback.Run(it->second, match_type) ==
        IterateCandidateResult::kFinish) {
      break;
    }
  }
}

// Get a PrefetchContainer from `prefetches` that can serve `key`, either:
// - Via exact match, or
// - Via No-Vary-Search information if exact match is not found, the feature is
// enabled and `SetNoVarySearchData()` is called for such `PrefetchContainer`s.
template <typename Value>
base::WeakPtr<PrefetchContainer> MatchUrl(
    const PrefetchKey& key,
    const std::map<PrefetchKey, Value>& prefetches) {
  base::WeakPtr<PrefetchContainer> result = nullptr;
  IterateCandidates(
      key, prefetches,
      base::BindRepeating(
          [](base::WeakPtr<PrefetchContainer>* result,
             const Value& prefetch_container, MatchType match_type) {
            switch (match_type) {
              case MatchType::kExact:
              case MatchType::kNoVarySearchHeader:
                // TODO(crbug.com/40064891): Revisit which PrefetchContainer to
                // return when there are multiple candidates. Currently we
                // return the first PrefetchContainer in URL lexicographic
                // order.
                *result = prefetch_container->GetWeakPtr();
                return IterateCandidateResult::kFinish;
              case MatchType::kNoVarySearchHint:
              case MatchType::kOther:
                return IterateCandidateResult::kContinue;
            }
          },
          base::Unretained(&result)));
  return result;
}

// Return the (URL,PrefetchContainer) pairs for a specific Url without
// query and reference. Allow as input urls with query and/or reference
// for ease of use (remove query/reference during lookup).
template <typename Value>
std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>
GetAllForUrlWithoutRefAndQueryForTesting(
    const PrefetchKey& key,
    const std::map<PrefetchKey, Value>& prefetches) {
  std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>> result;

  IterateCandidates(
      key, prefetches,
      base::BindRepeating(
          [](std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>*
                 result,
             const Value& prefetch_container, MatchType match_type) {
            result->emplace_back(prefetch_container->GetURL(),
                                 prefetch_container->GetWeakPtr());
            return IterateCandidateResult::kContinue;
          },
          base::Unretained(&result)));
  return result;
}

// Parse and return `HttpNoVarySearchData` from `head`, if any.
//
// On parse errors, send No-Vary-Search parsing errors in DevTools console.
// The method will test if there are errors/warning that the developer
// needs to know about, and if there are send them to the DevTools console.
std::optional<net::HttpNoVarySearchData> ProcessHead(
    const network::mojom::URLResponseHead& head,
    const GURL& url,
    RenderFrameHost* rfh);

// TODO(crbug.com/331591646): This is used in both prerender and prefetch,
// consider moving in a common location.
// Parse No-Vary-Search from mojom structure received from network service.
net::HttpNoVarySearchData ParseHttpNoVarySearchDataFromMojom(
    const network::mojom::NoVarySearchPtr& no_vary_search_ptr);

}  // namespace no_vary_search

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_NO_VARY_SEARCH_HELPER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_NO_VARY_SEARCH_HELPER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_NO_VARY_SEARCH_HELPER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/common/content_export.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/network/public/cpp/features.h"
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

// Sets `prefetch_container`'s `NoVarySearchData` based on the response header
// (`prefetch_container->GetHead()`) if applicable. Unless this is set, the
// helpers don't perform No-Vary-Search matching for `prefetch_container`, even
// if `GetHead()` has No-Vary-Search headers. If `prefetch_container` doesn't
// have a No-Vary-Search header this method is no-op and it is left to the
// caller to handle the case of true URL equality.
CONTENT_EXPORT void SetNoVarySearchData(
    base::WeakPtr<PrefetchContainer> prefetch_container);

// Indicates whether `IterateCandidates` should continue or finish after
// `callback` is called.
enum class IterateCandidateResult { kContinue, kFinish };

// Call `callback` on every `PrefetchContainer`s that can match with `url`, in
// the order of
// 1. Exact match.
// 2. No-Vary-Search matches if enabled.
//   - Has a URL with the same non-ref/query part as `url`,
//   - Has `NoVarySearchData`, AND
//   - `AreEquivalent()` is true or `check_are_equivalent` is false.
inline void IterateCandidates(
    const GURL& url,
    const std::map<GURL, base::WeakPtr<PrefetchContainer>>& prefetches,
    base::RepeatingCallback<
        IterateCandidateResult(base::WeakPtr<PrefetchContainer>)> callback,
    bool check_are_equivalent = true) {
  auto it_exact_match = prefetches.find(url);
  if (it_exact_match != prefetches.end() && it_exact_match->second) {
    if (callback.Run(it_exact_match->second) ==
        IterateCandidateResult::kFinish) {
      return;
    }
  }

  // Fall back to No-Vary-Search equivalence if enabled.
  if (!base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch)) {
    return;
  }

  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  GURL url_with_no_query = url.ReplaceComponents(replacements);

  // `std::map<GURL, ...>` is sorted by lexicographical string order of
  // the normalized URLs (`GURL::spec_`, i.e. `possibly_invalid_spec()`).
  // For a URL like `https://example.com/index.html?query#ref`, the
  // `lower_bound` call will get the first URL starting with
  // `https://example.com/index.html` (if any), and iterating by `++it` will get
  // the URLs starting with `https://example.com/index.html` in lexicographical
  // order until the URL without the `https://example.com/index.html` prefix is
  // encountered.
  for (auto it = prefetches.lower_bound(url_with_no_query);
       it != prefetches.end() && it->first.possibly_invalid_spec().starts_with(
                                     url_with_no_query.possibly_invalid_spec());
       ++it) {
    // `it_exact_match` is already visited above and thus skipped.
    if (it == it_exact_match) {
      continue;
    }

    if (!it->second) {
      continue;
    }

    if (!it->second->GetNoVarySearchData()) {
      continue;
    }
    // The URLs starting with `https://example.com/index.html` don't necessarily
    // have the same non-ref/query parts. See
    // `NoVarySearchHelperTest.DoNotPrefixMatch` unit tests for concrete
    // examples.
    if (it->first.ReplaceComponents(replacements) != url_with_no_query) {
      continue;
    }
    if (check_are_equivalent &&
        !it->second->GetNoVarySearchData()->AreEquivalent(url, it->first)) {
      continue;
    }

    if (callback.Run(it->second) == IterateCandidateResult::kFinish) {
      break;
    }
  }
}

// Get a PrefetchContainer from `prefetches` that can serve `url`, either:
// - Via exact match, or
// - Via No-Vary-Search information if exact match is not found, the feature is
// enabled and `SetNoVarySearchData()` is called for such `PrefetchContainer`s.
inline base::WeakPtr<PrefetchContainer> MatchUrl(
    const GURL& url,
    const std::map<GURL, base::WeakPtr<PrefetchContainer>>& prefetches) {
  base::WeakPtr<PrefetchContainer> result = nullptr;
  IterateCandidates(
      url, prefetches,
      base::BindRepeating(
          [](base::WeakPtr<PrefetchContainer>* result,
             base::WeakPtr<PrefetchContainer> prefetch_container) {
            // TODO(crbug.com/1449360): Revisit which PrefetchContainer to
            // return when there are multiple candidates. Currently we return
            // the first PrefetchContainer in URL lexicographic order.
            *result = std::move(prefetch_container);
            return IterateCandidateResult::kFinish;
          },
          base::Unretained(&result)));
  return result;
}

// Return the (URL,PrefetchContainer) pairs for a specific Url without
// query and reference. Allow as input urls with query and/or reference
// for ease of use (remove query/reference during lookup).
inline std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>
GetAllForUrlWithoutRefAndQueryForTesting(
    const GURL& url,
    const std::map<GURL, base::WeakPtr<PrefetchContainer>>& prefetches) {
  std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>> result;

  IterateCandidates(
      url, prefetches,
      base::BindRepeating(
          [](std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>*
                 result,
             base::WeakPtr<PrefetchContainer> prefetch_container) {
            result->emplace_back(prefetch_container->GetURL(),
                                 prefetch_container);
            return IterateCandidateResult::kContinue;
          },
          base::Unretained(&result)),
      false /* check_are_equivalent */
  );
  return result;
}

// Send No-Vary-Search parsing errors in DevTools console.
// The method will test if there are errors/warning that the developer
// needs to know about, and if there are send them to the DevTools console.
void MaybeSendErrorsToConsole(const GURL& url,
                              const network::mojom::URLResponseHead& head,
                              RenderFrameHost& rfh);

// Parse No-Vary-Search from mojom structure received from network service.
net::HttpNoVarySearchData ParseHttpNoVarySearchDataFromMojom(
    const network::mojom::NoVarySearchPtr& no_vary_search_ptr);

}  // namespace no_vary_search

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_NO_VARY_SEARCH_HELPER_H_

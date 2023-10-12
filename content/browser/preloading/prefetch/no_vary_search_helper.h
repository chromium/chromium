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
#include "content/public/browser/global_routing_id.h"
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

// See comments inside `IterateCandidates()` for requirements for `PrefetchKey`.
template <typename PrefetchKey>
class PrefetchKeyTraits;

template <>
class PrefetchKeyTraits<GURL> {
 public:
  using PrefetchKey = GURL;
  static const GURL& GetURL(const PrefetchKey& key) { return key; }
  static PrefetchKey KeyWithNewURL(const PrefetchKey& old_key,
                                   const GURL& new_url) {
    return new_url;
  }
  static bool NonUrlPartIsSame(const PrefetchKey& key1,
                               const PrefetchKey& key2) {
    return true;
  }
};

template <typename T>
class PrefetchKeyTraits<std::pair<T, GURL>> {
 public:
  using PrefetchKey = std::pair<T, GURL>;
  static const GURL& GetURL(const PrefetchKey& key) { return key.second; }
  static PrefetchKey KeyWithNewURL(const PrefetchKey& old_key,
                                   const GURL& new_url) {
    return PrefetchKey(old_key.first, new_url);
  }
  static bool NonUrlPartIsSame(const PrefetchKey& key1,
                               const PrefetchKey& key2) {
    return key1.first == key2.first;
  }
};

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
template <typename PrefetchKey>
void IterateCandidates(
    const PrefetchKey& key,
    const std::map<PrefetchKey, base::WeakPtr<PrefetchContainer>>& prefetches,
    base::RepeatingCallback<
        IterateCandidateResult(base::WeakPtr<PrefetchContainer>)> callback,
    bool check_are_equivalent = true) {
  auto it_exact_match = prefetches.find(key);
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
  const GURL& key_url = PrefetchKeyTraits<PrefetchKey>::GetURL(key);
  GURL url_with_no_query = key_url.ReplaceComponents(replacements);

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
  // placed in the `std::map<PrefetchKey, ...>` iteration order. `GURL` and
  // `std::pair<T, GURL>` satisfies this requirement and thus corresponding
  // `PrefetchKeyTraits` are defined, while e.g. `std::pair<GURL, T>` wouldn't
  // work.
  //
  // The same applies to `std::map<std::pair<DocumentToken, GURL>, ...>`, as
  // URLs within the same `DocumentToken` is sorted in the same way.
  // An additional check of `DocumentToken` is needed to ensure we still
  // iterating URLs within the same `DocumentToken`, which is done in
  // `NonUrlPartIsSame()`.
  for (auto it =
           prefetches.lower_bound(PrefetchKeyTraits<PrefetchKey>::KeyWithNewURL(
               key, url_with_no_query));
       it != prefetches.end(); ++it) {
    const GURL& prefetch_container_url =
        PrefetchKeyTraits<PrefetchKey>::GetURL(it->first);

    if (!PrefetchKeyTraits<PrefetchKey>::NonUrlPartIsSame(key, it->first) ||
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

    if (!it->second->GetNoVarySearchData()) {
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
    if (check_are_equivalent &&
        !it->second->GetNoVarySearchData()->AreEquivalent(
            key_url, prefetch_container_url)) {
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
template <typename PrefetchKey>
base::WeakPtr<PrefetchContainer> MatchUrl(
    const PrefetchKey& key,
    const std::map<PrefetchKey, base::WeakPtr<PrefetchContainer>>& prefetches) {
  base::WeakPtr<PrefetchContainer> result = nullptr;
  IterateCandidates(
      key, prefetches,
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
template <typename PrefetchKey>
std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>
GetAllForUrlWithoutRefAndQueryForTesting(
    const PrefetchKey& key,
    const std::map<PrefetchKey, base::WeakPtr<PrefetchContainer>>& prefetches) {
  std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>> result;

  IterateCandidates(
      key, prefetches,
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

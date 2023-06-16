// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/no_vary_search_helper.h"

#include <map>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/no_vary_search_header_parser.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace no_vary_search {

namespace {

// Indicates whether `IterateCandidates` should continue or finish after
// `callback` is called.
enum class IterateCandidateResult { kContinue, kFinish };

// Call `callback` on every `PrefetchContainer`s that can match with `url` via
// No-Vary-Search:
// - Has a URL with the same non-ref/query part as `url`,
// - Has `NoVarySearchData`, AND
// - `AreEquivalent()` is true or `check_are_equivalent` is false.
// Note that if `PrefetchContainer` doesn't have a valid `NoVarySearchData`, it
// is ignored even if its URL is exactly the same as `url`.
void IterateCandidates(
    const GURL& url,
    const std::map<GURL, base::WeakPtr<PrefetchContainer>>& prefetches,
    base::RepeatingCallback<
        IterateCandidateResult(base::WeakPtr<PrefetchContainer>)> callback,
    bool check_are_equivalent = true) {
  DCHECK(
      base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch));

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

}  // namespace

// static
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

void SetNoVarySearchData(base::WeakPtr<PrefetchContainer> prefetch_container) {
  DCHECK(
      base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch));
  // Check if the prefetched response has a No-Vary-Search header and
  // call SetNoVarySearchData() so that it can be looked up by
  // IterateCandidates().
  const network::mojom::URLResponseHead& head = *prefetch_container->GetHead();
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
  prefetch_container->SetNoVarySearchData(std::move(no_vary_search_data));
}

void MaybeSendErrorsToConsole(const GURL& url,
                              const network::mojom::URLResponseHead& head,
                              RenderFrameHost& rfh) {
  DCHECK(
      base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch));
  if (!(head.parsed_headers &&
        head.parsed_headers->no_vary_search_with_parse_error)) {
    return;
  }
  if (!head.parsed_headers->no_vary_search_with_parse_error->is_parse_error()) {
    return;
  }
  const auto parse_error =
      head.parsed_headers->no_vary_search_with_parse_error->get_parse_error();
  if (parse_error == network::mojom::NoVarySearchParseError::kOk) {
    return;
  }
  blink::mojom::ConsoleMessageLevel error_level =
      parse_error == network::mojom::NoVarySearchParseError::kDefaultValue
          ? blink::mojom::ConsoleMessageLevel::kWarning
          : blink::mojom::ConsoleMessageLevel::kError;
  auto error_message = network::GetNoVarySearchConsoleMessage(parse_error, url);
  CHECK(error_message);
  rfh.AddMessageToConsole(error_level, error_message.value());
}

base::WeakPtr<PrefetchContainer> MatchUrl(
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

std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>
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

}  // namespace no_vary_search

}  // namespace content

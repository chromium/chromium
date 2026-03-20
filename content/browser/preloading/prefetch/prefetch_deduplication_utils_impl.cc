// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/prefetch_deduplication_utils.h"

namespace content {

bool IsPrefetchDuplicate(
    const std::vector<const PrefetchDeduplicationEntry*>& prefetch_entries,
    const GURL& url,
    const std::optional<net::HttpNoVarySearchData>& no_vary_search_hint) {
  for (const auto* prefetch_entry : prefetch_entries) {
    CHECK(prefetch_entry);

    if (prefetch_entry->IsPrefetchStale()) {
      continue;
    }

    // We will only compare the URLs if the no-vary-search hints match for
    // determinism. This is because comparing URLs with different no-vary-search
    // hints will change the outcome of the comparison based on the order the
    // requests happened in.
    //
    // This approach optimizes for determinism over minimizing wasted
    // or redundant prefetches.
    bool nvs_hints_match =
        no_vary_search_hint == prefetch_entry->GetNoVarySearchHint();
    if (!nvs_hints_match) {
      continue;
    }

    bool urls_equal;
    if (no_vary_search_hint) {
      urls_equal =
          no_vary_search_hint->AreEquivalent(url, prefetch_entry->GetURL());
    } else {
      // If there is no no-vary-search hint, just compare the URLs.
      urls_equal = url == prefetch_entry->GetURL();
    }

    if (!urls_equal) {
      continue;
    }
    return true;
  }
  return false;
}

}  // namespace content

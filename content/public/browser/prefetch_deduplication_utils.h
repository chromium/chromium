// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PREFETCH_DEDUPLICATION_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_PREFETCH_DEDUPLICATION_UTILS_H_

#include <optional>
#include <vector>

#include "content/common/content_export.h"
#include "net/http/http_no_vary_search_data.h"
#include "url/gurl.h"

// This utility is for deduplicating prefetching requests before entering
// `PrefetchService::AddPrefetchRequestInternal()`.
// The deduplication here is based on URL and No-Vary-Search hint, and doesn't
// use actual No-Vary-Search response headers received.
// TODO(crbug.com/490251714): Consider unifying prefetch deduplication
// mechanisms with NVS and moving it into `content/browser/preloading`.
namespace content {

// A class that represents a prefetch entry including the requirements for a
// prefetch entry to be checked for duplication.
class CONTENT_EXPORT PrefetchDeduplicationEntry {
 public:
  virtual ~PrefetchDeduplicationEntry() = default;
  // The initial URL for this prefetch.
  virtual const GURL& GetURL() const = 0;
  // The No-Vary-Search hint associated with this prefetch.
  virtual const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchHint()
      const = 0;
  // Whether or not the prefetch is regarded as stale in the context of each
  // prefetch store.
  virtual bool IsPrefetchStale() const = 0;
};

// Checks if a given request is a duplicate of any existing prefetch entries.
CONTENT_EXPORT bool IsPrefetchDuplicate(
    const std::vector<const PrefetchDeduplicationEntry*>& prefetch_entries,
    const GURL& url,
    const std::optional<net::HttpNoVarySearchData>& no_vary_search_hint);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PREFETCH_DEDUPLICATION_UTILS_H_

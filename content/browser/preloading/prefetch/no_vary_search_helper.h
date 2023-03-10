// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_NO_VARY_SEARCH_HELPER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_NO_VARY_SEARCH_HELPER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "net/http/http_no_vary_search_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;
namespace network::mojom {
class URLResponseHead;
}  // namespace network::mojom

namespace content {

class RenderFrameHost;

// Helper class to keep track of prefetched URLs that have No-Vary-Search
// header present in their responses.
class CONTENT_EXPORT NoVarySearchHelper
    : public base::RefCounted<NoVarySearchHelper> {
 public:
  NoVarySearchHelper();

  // Track `url` with No-Vary-Search header information if applicable.
  // If `url` doesn't have a No-Vary-Search header this method will not
  // track `url` at all. It is left to the caller to handle the case of
  // true URL equality.
  void AddUrl(const GURL& url, const network::mojom::URLResponseHead& head);

  // Try to match `url` within tracked urls with No-Vary-Search information.
  // Return the matched url or absl::nullopt otherwise.
  absl::optional<GURL> MatchUrl(const GURL& url) const;

  // Send No-Vary-Search parsing errors in DevTools console.
  // The method will test if there are errors/warning that the developer
  // needs to know about, and if there are send them to the DevTools console.
  void MaybeSendErrorsToConsole(const GURL& url,
                                const network::mojom::URLResponseHead& head,
                                RenderFrameHost& rfh) const;

  // Return the (URL,NoVarySearchInfo) pairs for a specific Url without
  // query and reference. Allow as input urls with query and/or reference
  // for ease of use (remove query/reference during lookup).
  const std::vector<std::pair<GURL, net::HttpNoVarySearchData>>*
  GetAllForUrlWithoutRefAndQueryForTesting(const GURL& url) const;

 private:
  friend class base::RefCounted<NoVarySearchHelper>;
  ~NoVarySearchHelper();

  // The set of urls that have No-Vary-Search header in their prefetched
  // response keyed by their path without the ref and query parts.
  std::map<GURL, std::vector<std::pair<GURL, net::HttpNoVarySearchData>>>
      prefetches_with_no_vary_search_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_NO_VARY_SEARCH_HELPER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_NO_VARY_SEARCH_HELPER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_NO_VARY_SEARCH_HELPER_H_

#include <map>
#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;
namespace network::mojom {
class URLResponseHead;
}  // namespace network::mojom

namespace content {

class PrefetchContainer;
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

// Get a PrefetchContainer from `prefetches` that can serve `url` according to
// No-Vary-Search information.
CONTENT_EXPORT base::WeakPtr<PrefetchContainer> MatchUrl(
    const GURL& url,
    const std::map<GURL, base::WeakPtr<PrefetchContainer>>& prefetches);

// Send No-Vary-Search parsing errors in DevTools console.
// The method will test if there are errors/warning that the developer
// needs to know about, and if there are send them to the DevTools console.
void MaybeSendErrorsToConsole(const GURL& url,
                              const network::mojom::URLResponseHead& head,
                              RenderFrameHost& rfh);

// Return the (URL,PrefetchContainer) pairs for a specific Url without
// query and reference. Allow as input urls with query and/or reference
// for ease of use (remove query/reference during lookup).
CONTENT_EXPORT std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>
GetAllForUrlWithoutRefAndQueryForTesting(
    const GURL& url,
    const std::map<GURL, base::WeakPtr<PrefetchContainer>>& prefetches);

// Parse No-Vary-Search from mojom structure received from network service.
net::HttpNoVarySearchData ParseHttpNoVarySearchDataFromMojom(
    const network::mojom::NoVarySearchPtr& no_vary_search_ptr);

}  // namespace no_vary_search

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_NO_VARY_SEARCH_HELPER_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_RESOURCE_REQUEST_UTILS_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_RESOURCE_REQUEST_UTILS_H_

#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"

namespace content {

struct PrefetchUpdateHeadersParams;

// Avoid using `inline constexpr` here in order to place the definition to
// `.cc` file to get `tools/traffic_annotation/scripts/auditor/auditor.py` to
// work (See crbug.com/484967082 for more details).
extern const net::NetworkTrafficAnnotationTag
    kNavigationalPrefetchTrafficAnnotation;

// ------------------------------------------------------------------------
// Utilities for constructing request headers.
// Header modifications should be applied in the following order, and the
// latter (if any) should override the former.
// [1] `request().additional_headers()`
// [2] Chromium's default headers
// [3] WebContents overrides
// [4] DevTools overrides

// Returns a `PrefetchUpdateHeadersParams` that contains the headers to be added
// to the initial prefetch `ResourceRequest`.
PrefetchUpdateHeadersParams PrepareInitialHeadersForPrefetch(
    const GURL& request_url,
    const PrefetchRequest& prefetch_request,
    bool is_first_party_context_for_variations_header);

// Returns a tuple of `PrefetchUpdateHeadersParams`s that indicates the header
// modification upon redirect, to be passed to
// `PrefetchContainer::UpdateResourceRequest()` and
// `URLLoader::FollowRedirect()`, respectively.
// TODO(crbug.com/467177773): Ideally these two should be equal, but currently
// we are incrementally adding headers to the latter.
std::tuple<PrefetchUpdateHeadersParams, PrefetchUpdateHeadersParams>
PrepareRedirectHeadersForPrefetch(const GURL& request_url,
                                  const PrefetchRequest& prefetch_request);

// Adds "X-Client-Data" header for a prefetch request to `request_url`.
// `cors_exempt_headers` corresponds to `ResourceRequest::cors_exempt_headers`.
// Note that `request_url` and `prefetch_request.url` / `resource_request`
// (that `request_headers` belongs)'s `url` can be different when called from
// `PrefetchContainer::PrepareUpdateHeaders()`.
void AddVariationsHeaderForPrefetch(
    net::HttpRequestHeaders& cors_exempt_headers,
    const GURL& request_url,
    const PrefetchRequest& prefetch_request,
    bool is_first_party_context_for_variations);

mojo::PendingRemote<network::mojom::DevToolsObserver>
MaybeMakeSelfOwnedNetworkServiceDevToolsObserverForPrefetch(
    const PrefetchRequest& prefetch_request);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_RESOURCE_REQUEST_UTILS_H_

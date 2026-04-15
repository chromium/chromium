// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_RESOURCE_REQUEST_UTILS_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_RESOURCE_REQUEST_UTILS_H_

#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/common/content_export.h"
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
//
// Header modifications should be applied in the following order, and the
// latter (if any) should override the former.
// [1] `request().additional_headers()`
// [2] Chromium's default headers
// [3] WebContents overrides
// [4] DevTools overrides
//
// `request_url` is the next URL to prefetch (the initial prefetch URL or the
// URL after redirect).
//
// While the full URL is passed here, only the origin of the URL is used, except
// for `WebContentsDelegate::ShouldOverrideUserAgentForPreloading()` used for
// the WebContents `User-Agent` override.

// Returns a `PrefetchUpdateHeadersParams` that contains the headers to be added
// to the initial prefetch `ResourceRequest`.

// Header constructions are split into two phases, due to the different nature
// of the dependencies to the UI thread, and in order to apply Phase 1 after
// receiving actual `PrefetchRequest`s to make more headers reflect actual
// `PrefetchRequest`s.
// The two `PrefetchUpdateHeadersParams` must be applied in order.

// Phase 1:
// - [1] and part of [2].
// - Can be executed on any thread.
PrefetchUpdateHeadersParams PrepareInitialHeadersForPrefetchPhase1(
    const GURL& request_url,
    const PrefetchRequest& prefetch_request);
// Phase 2:
// - [2], [3] and [4].
// - Must be executed only on the UI thread.
PrefetchUpdateHeadersParams PrepareInitialHeadersForPrefetchPhase2(
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

// Modifies "X-Client-Data" headers of `resource_request` upon redirect. Must be
// called after `resource_request.url` is updated.
void UpdateVariationsHeaderForPrefetch(
    network::ResourceRequest& resource_request,
    const PrefetchRequest& prefetch_request);

// ------------------------------------------------------------------------
// Utilities for constructing `network::ResourceRequest`.

// Constructs a full `ResourceRequest`, based on
// `MakeInitialResourceRequestWithoutHeadersForPrefetch()` and
// `PrepareInitialHeadersForPrefetch()`.
CONTENT_EXPORT std::unique_ptr<network::ResourceRequest>
MakeInitialResourceRequestForPrefetch(const PrefetchRequest& prefetch_request,
                                      bool is_decoy);

// Constructs a full `ResourceRequest` for PrePrefetch, using the
// pre-calculated headers on the UI thread via
// `PrepareInitialHeadersForPrefetch()`, and
// `MakeInitialResourceRequestWithoutHeadersForPrefetch()`.
CONTENT_EXPORT std::unique_ptr<network::ResourceRequest>
MakeInitialResourceRequestForPrePrefetch(
    const PrefetchRequest& prefetch_request,
    const PrefetchUpdateHeadersParams& ui_thread_pre_calculated_headers);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_RESOURCE_REQUEST_UTILS_H_

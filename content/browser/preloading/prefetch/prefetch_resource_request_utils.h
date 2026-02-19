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

// Avoid using `inline constexpr` here in order to place the definition to
// `.cc` file to get `tools/traffic_annotation/scripts/auditor/auditor.py` to
// work (See crbug.com/484967082 for more details).
extern const net::NetworkTrafficAnnotationTag
    kNavigationalPrefetchTrafficAnnotation;

// Returns "Sec-Purpose" header value for a prefetch request to `request_url`.
// Note that `request_url` and `prefetch_request.url` / `resource_request`
// (that `request_headers` belongs)'s `url` can be different when called from
// `PrefetchContainer::PrepareUpdateHeaders()`.
void AddSecPurposeHeader(net::HttpRequestHeaders& request_headers,
                         const GURL& request_url,
                         const PrefetchRequest& prefetch_request);

// Adds Speculation Rules Tags headers for a prefetch request to `request_url`
// to `headers`.
// Note that `request_url` and `prefetch_request.url` / `resource_request`
// (that `request_headers` belongs)'s `url` can be different when called from
// `PrefetchContainer::PrepareUpdateHeaders()`.
void AddSpeculationTagsHeader(net::HttpRequestHeaders& request_headers,
                              const GURL& request_url,
                              const PrefetchRequest& prefetch_request);

// TODO(crbug.com/483079815): We won't need to expose this once we
// move the whole `MakeInitialResourceRequest` to here.
void AddAdditionalHeaders(net::HttpRequestHeaders& request_headers,
                          const PrefetchRequest& prefetch_request);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_RESOURCE_REQUEST_UTILS_H_

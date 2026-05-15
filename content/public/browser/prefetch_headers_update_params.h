// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PREFETCH_HEADERS_UPDATE_PARAMS_H_
#define CONTENT_PUBLIC_BROWSER_PREFETCH_HEADERS_UPDATE_PARAMS_H_

#include "base/functional/callback.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"

namespace network {
struct ResourceRequest;
}

namespace content {

// A Callback type to use for modifying the Prefetch request headers per const
// `ResourceRequest`.
// For PrePrefetch requests, the returned `HttpRequestHeadersUpdateParams` is
// applied to the initial request as the last step of request creation, and
// conceptually emulates the header modification that would be performed in
// `ContentBrowserClient::WillCreateURLLoaderFactory`.
using PrePrefetchUpdateHeadersCallback =
    base::RepeatingCallback<network::HttpRequestHeadersUpdateParams(
        const network::ResourceRequest&)>;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PREFETCH_HEADERS_UPDATE_PARAMS_H_

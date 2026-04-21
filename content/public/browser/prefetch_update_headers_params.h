// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PREFETCH_UPDATE_HEADERS_PARAMS_H_
#define CONTENT_PUBLIC_BROWSER_PREFETCH_UPDATE_HEADERS_PARAMS_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "net/http/http_request_headers.h"
#include "url/origin.h"

namespace network {
struct ResourceRequest;
}

namespace content {

// Indicates the modification to the network resource request of Prefetch e.g.
// - Upon Prefetch redirect, where it should be applied to
//   `PrefetchContainer::resource_request_` or passed to `FollowRedirect()`.
// - PrePrefetch initial request header modification from embedder after the
//   initial resource request is created, which conceptually emulates the
//   header modification that would be performed in
//   `ContentBrowserClient::WillCreateURLLoaderFactory`. This is embedded via
//   `PrePrefetchUpdateHeadersCallback`.
//
// TODO(crbug.com/452389538): We can revisit and investigate necessary and
// sufficient interface that can be commonly used for similar header updates
// beyond Prefetch.
struct CONTENT_EXPORT PrefetchUpdateHeadersParams final {
  PrefetchUpdateHeadersParams();
  ~PrefetchUpdateHeadersParams();
  PrefetchUpdateHeadersParams(PrefetchUpdateHeadersParams&&);
  PrefetchUpdateHeadersParams& operator=(PrefetchUpdateHeadersParams&&);
  PrefetchUpdateHeadersParams(const PrefetchUpdateHeadersParams&) = delete;
  PrefetchUpdateHeadersParams& operator=(const PrefetchUpdateHeadersParams&) =
      delete;

  std::vector<std::string> removed_headers;
  net::HttpRequestHeaders modified_headers;
  net::HttpRequestHeaders modified_cors_exempt_headers;
};

// A Callback type to use for modifying the Prefetch request headers per const
// `ResourceRequest`. See the above struct comment as well.
using PrePrefetchUpdateHeadersCallback =
    base::RepeatingCallback<PrefetchUpdateHeadersParams(
        const network::ResourceRequest&)>;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PREFETCH_UPDATE_HEADERS_PARAMS_H_

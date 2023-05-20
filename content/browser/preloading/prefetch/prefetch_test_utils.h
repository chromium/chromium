// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TEST_UTILS_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TEST_UTILS_H_

#include <memory>
#include <string>

#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace content {

class PrefetchStreamingURLLoader;

std::unique_ptr<PrefetchStreamingURLLoader>
MakeServableStreamingURLLoaderForTest(network::mojom::URLResponseHeadPtr head,
                                      const std::string body);

std::unique_ptr<PrefetchStreamingURLLoader>
MakeServableStreamingURLLoaderWithRedirectForTest(const GURL& original_url,
                                                  const GURL& redirect_url);

std::vector<std::unique_ptr<PrefetchStreamingURLLoader>>
MakeServableStreamingURLLoadersWithNetworkTransitionRedirectForTest(
    const GURL& original_url,
    const GURL& redirect_url);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TEST_UTILS_H_

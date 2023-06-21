// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TEST_UTILS_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TEST_UTILS_H_

#include <memory>
#include <string>

#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace base {
class RunLoop;
}  // namespace base

namespace content {

class PrefetchContainer;

void MakeServableStreamingURLLoaderForTest(
    PrefetchContainer* prefetch_container,
    network::mojom::URLResponseHeadPtr head,
    const std::string body);

PrefetchStreamingURLLoader::OnPrefetchRedirectCallback
CreatePrefetchRedirectCallbackForTest(
    base::RunLoop* on_receive_redirect_loop,
    net::RedirectInfo* out_redirect_info,
    network::mojom::URLResponseHeadPtr* out_redirect_head);

void MakeServableStreamingURLLoaderWithRedirectForTest(
    PrefetchContainer* prefetch_container,
    const GURL& original_url,
    const GURL& redirect_url);

std::vector<base::WeakPtr<PrefetchStreamingURLLoader>>
MakeServableStreamingURLLoadersWithNetworkTransitionRedirectForTest(
    PrefetchContainer* prefetch_container,
    const GURL& original_url,
    const GURL& redirect_url);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TEST_UTILS_H_

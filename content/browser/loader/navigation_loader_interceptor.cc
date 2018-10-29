// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_loader_interceptor.h"

#include "content/common/navigation_subresource_loader_params.h"

namespace content {

base::Optional<SubresourceLoaderParams>
NavigationLoaderInterceptor::MaybeCreateSubresourceLoaderParams() {
  return base::nullopt;
}

bool NavigationLoaderInterceptor::MaybeCreateLoaderForResponse(
    const GURL& request_url,
    const network::ResourceResponseHead& response,
    network::mojom::URLLoaderPtr* loader,
    network::mojom::URLLoaderClientRequest* client_request,
    ThrottlingURLLoader* url_loader,
    bool* skip_other_interceptors) {
  return false;
}

}  // namespace content

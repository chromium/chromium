// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_loader_interceptor.h"

#include "content/browser/navigation_subresource_loader_params.h"

namespace content {

base::Optional<SubresourceLoaderParams>
NavigationLoaderInterceptor::MaybeCreateSubresourceLoaderParams() {
  return base::nullopt;
}

bool NavigationLoaderInterceptor::MaybeCreateLoaderForResponse(
    const network::ResourceRequest& request,
    const network::ResourceResponseHead& response,
    mojo::ScopedDataPipeConsumerHandle* response_body,
    network::mojom::URLLoaderPtr* loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
    blink::ThrottlingURLLoader* url_loader,
    bool* skip_other_interceptors,
    bool* will_return_unsafe_redirect) {
  return false;
}

bool NavigationLoaderInterceptor::ShouldBypassRedirectChecks() {
  return false;
}

}  // namespace content

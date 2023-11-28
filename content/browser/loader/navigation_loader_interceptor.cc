// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_loader_interceptor.h"

#include "content/browser/navigation_subresource_loader_params.h"

namespace content {

absl::optional<SubresourceLoaderParams>
NavigationLoaderInterceptor::MaybeCreateSubresourceLoaderParams() {
  return absl::nullopt;
}

bool NavigationLoaderInterceptor::MaybeCreateLoaderForResponse(
    const network::URLLoaderCompletionStatus& status,
    const network::ResourceRequest& request,
    network::mojom::URLResponseHeadPtr* response,
    mojo::ScopedDataPipeConsumerHandle* response_body,
    mojo::PendingRemote<network::mojom::URLLoader>* loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
    blink::ThrottlingURLLoader* url_loader,
    bool* skip_other_interceptors) {
  return false;
}

}  // namespace content

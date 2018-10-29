// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/not_implemented_network_url_loader_factory.h"

namespace content {

NotImplementedNetworkURLLoaderFactory::NotImplementedNetworkURLLoaderFactory() =
    default;

NotImplementedNetworkURLLoaderFactory::
    ~NotImplementedNetworkURLLoaderFactory() = default;

void NotImplementedNetworkURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  network::URLLoaderCompletionStatus status;
  status.error_code = net::ERR_NOT_IMPLEMENTED;
  client->OnComplete(status);
}

void NotImplementedNetworkURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace content

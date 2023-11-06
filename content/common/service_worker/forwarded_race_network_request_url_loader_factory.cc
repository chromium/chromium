// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/forwarded_race_network_request_url_loader_factory.h"

namespace content {

ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::
    ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory(
        mojo::PendingReceiver<network::mojom::URLLoaderClient> client_receiver,
        scoped_refptr<network::SharedURLLoaderFactory> fallback_factory)
    : client_receiver_(std::move(client_receiver)),
      fallback_factory_(fallback_factory) {}

ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::
    ~ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory() = default;

void ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::
    CreateLoaderAndStart(
        mojo::PendingReceiver<network::mojom::URLLoader> receiver,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& resource_request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (!is_data_pipe_fused_) {
    // If the member data pipes are still not fused to mojo endpoints, fuse them
    // to reuse the response.
    bool result =
        mojo::FusePipes(std::move(client_receiver_), std::move(client));
    CHECK(result) << resource_request.url;
    result = mojo::FusePipes(std::move(receiver), std::move(loader_));
    CHECK(result) << resource_request.url;
    is_data_pipe_fused_ = true;
  } else {
    // If already fused, create a new URLLoader and start the new request.
    fallback_factory_->CreateLoaderAndStart(
        std::move(receiver), request_id, options, resource_request,
        std::move(client), traffic_annotation);
  }
}

void ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receiver_.Bind(std::move(receiver));
}

mojo::PendingReceiver<network::mojom::URLLoader>
ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::
    InitURLLoaderNewPipeAndPassReceiver() {
  return loader_.InitWithNewPipeAndPassReceiver();
}
}  // namespace content

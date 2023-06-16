// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/forwarded_race_network_request_url_loader_factory.h"

namespace content {

ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::
    ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory(
        mojo::PendingReceiver<network::mojom::URLLoaderClient> client_receiver,
        const GURL& url)
    : client_receiver_(std::move(client_receiver)), url_(url) {}

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
  CHECK_EQ(url_, resource_request.url);
  bool result = mojo::FusePipes(std::move(client_receiver_), std::move(client));
  CHECK(result);
}

void ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receiver_.Bind(std::move(receiver));
}
}  // namespace content

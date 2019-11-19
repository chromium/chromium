// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/not_implemented_network_url_loader_factory.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

NotImplementedNetworkURLLoaderFactory::NotImplementedNetworkURLLoaderFactory() =
    default;

NotImplementedNetworkURLLoaderFactory::
    ~NotImplementedNetworkURLLoaderFactory() = default;

void NotImplementedNetworkURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  network::URLLoaderCompletionStatus status;
  status.error_code = net::ERR_NOT_IMPLEMENTED;
  mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
      ->OnComplete(status);
}

void NotImplementedNetworkURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace content

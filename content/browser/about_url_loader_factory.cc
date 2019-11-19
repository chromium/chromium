// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/about_url_loader_factory.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

AboutURLLoaderFactory::AboutURLLoaderFactory() = default;
AboutURLLoaderFactory::~AboutURLLoaderFactory() = default;

void AboutURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  network::ResourceResponseHead response_head;
  response_head.mime_type = "text/html";
  mojo::Remote<network::mojom::URLLoaderClient> client_remote(
      std::move(client));
  client_remote->OnReceiveResponse(response_head);

  // Create a data pipe for transmitting the empty response. The |producer|
  // doesn't add any data.
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (CreateDataPipe(nullptr, &producer, &consumer) != MOJO_RESULT_OK) {
    client_remote->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }

  client_remote->OnStartLoadingResponseBody(std::move(consumer));
  client_remote->OnComplete(network::URLLoaderCompletionStatus(net::OK));
}

void AboutURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader) {
  receivers_.Add(this, std::move(loader));
}

}  // namespace content

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/about_url_loader_factory.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

AboutURLLoaderFactory::AboutURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
    : network::SelfDeletingURLLoaderFactory(std::move(factory_receiver)) {}

AboutURLLoaderFactory::~AboutURLLoaderFactory() = default;

void AboutURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->mime_type = "text/html";
  mojo::Remote<network::mojom::URLLoaderClient> client_remote(
      std::move(client));

  // Create a data pipe for transmitting the empty response. The |producer|
  // doesn't add any data.
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (CreateDataPipe(nullptr, producer, consumer) != MOJO_RESULT_OK) {
    client_remote->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }

  client_remote->OnReceiveResponse(std::move(response_head),
                                   std::move(consumer), std::nullopt);
  client_remote->OnComplete(network::URLLoaderCompletionStatus(net::OK));
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
AboutURLLoaderFactory::Create() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

  // The AboutURLLoaderFactory will delete itself when there are no more
  // receivers - see the network::SelfDeletingURLLoaderFactory::OnDisconnect
  // method.
  new AboutURLLoaderFactory(pending_remote.InitWithNewPipeAndPassReceiver());

  return pending_remote;
}

}  // namespace content

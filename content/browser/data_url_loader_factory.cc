// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/data_url_loader_factory.h"

#include <string_view>

#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/data_url.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

namespace {
struct WriteData {
  mojo::Remote<network::mojom::URLLoaderClient> client;
  std::string data;
  std::unique_ptr<mojo::DataPipeProducer> producer;
};

void OnWrite(std::unique_ptr<WriteData> write_data, MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    write_data->client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = write_data->data.size();
  status.encoded_body_length = write_data->data.size();
  status.decoded_body_length = write_data->data.size();
  write_data->client->OnComplete(status);
}

}  // namespace

DataURLLoaderFactory::DataURLLoaderFactory(
    const GURL& url,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
    : network::SelfDeletingURLLoaderFactory(std::move(factory_receiver)),
      url_(url) {}

DataURLLoaderFactory::~DataURLLoaderFactory() = default;

void DataURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  const GURL* url = nullptr;
  if (!url_.is_empty() && request.url.is_empty()) {
    url = &url_;
  } else {
    url = &request.url;
  }

  std::string data;
  scoped_refptr<net::HttpResponseHeaders> headers;
  auto response = network::mojom::URLResponseHead::New();
  net::Error result = net::DataURL::BuildResponse(
      *url, request.method, &response->mime_type, &response->charset, &data,
      &response->headers);

  // Users of CreateAndBindForOneSpecificUrl should only submit one load
  // request - we won't need the URL anymore.
  url_ = GURL();

  mojo::Remote<network::mojom::URLLoaderClient> client_remote(
      std::move(client));
  if (result != net::OK) {
    client_remote->OnComplete(network::URLLoaderCompletionStatus(result));
    return;
  }

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (CreateDataPipe(nullptr, producer, consumer) != MOJO_RESULT_OK) {
    client_remote->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }

  client_remote->OnReceiveResponse(std::move(response), std::move(consumer),
                                   std::nullopt);

  auto write_data = std::make_unique<WriteData>();
  write_data->client = std::move(client_remote);
  write_data->data = std::move(data);
  write_data->producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(producer));

  mojo::DataPipeProducer* producer_ptr = write_data->producer.get();
  std::string_view string_piece(write_data->data);

  producer_ptr->Write(
      std::make_unique<mojo::StringDataSource>(
          string_piece, mojo::StringDataSource::AsyncWritingMode::
                            STRING_STAYS_VALID_UNTIL_COMPLETION),
      base::BindOnce(OnWrite, std::move(write_data)));
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
DataURLLoaderFactory::Create() {
  return CreateForOneSpecificUrl(GURL());
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
DataURLLoaderFactory::CreateForOneSpecificUrl(const GURL& url) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

  // The DataURLLoaderFactory will delete itself when there are no more
  // receivers - see the network::SelfDeletingURLLoaderFactory::OnDisconnect
  // method.
  new DataURLLoaderFactory(url,
                           pending_remote.InitWithNewPipeAndPassReceiver());

  return pending_remote;
}

}  // namespace content

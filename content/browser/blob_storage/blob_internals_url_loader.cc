// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/blob_internals_url_loader.h"

#include "base/containers/span.h"
#include "content/browser/blob_storage/blob_internals_url_loader.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "storage/browser/blob/view_blob_internals_job.h"

namespace content {

void StartBlobInternalsURLLoader(
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    ChromeBlobStorageContext* blob_storage_context) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  auto resource_response = network::mojom::URLResponseHead::New();
  resource_response->headers = headers;
  resource_response->mime_type = "text/html";

  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(client_remote));

  std::string output = storage::ViewBlobInternalsJob::GenerateHTML(
      blob_storage_context->context());
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CHECK_EQ(
      mojo::CreateDataPipe(output.size(), producer_handle, consumer_handle),
      MOJO_RESULT_OK);

  base::span<uint8_t> buffer;
  size_t size_hint = output.size();
  MojoResult result = producer_handle->BeginWriteData(
      size_hint, MOJO_BEGIN_WRITE_DATA_FLAG_ALL_OR_NONE, buffer);
  CHECK_EQ(result, MOJO_RESULT_OK);

  CHECK_GE(buffer.size(), output.size());
  buffer = buffer.first(output.size());

  buffer.copy_from(base::as_byte_span(output));
  result = producer_handle->EndWriteData(buffer.size());
  CHECK_EQ(result, MOJO_RESULT_OK);

  client->OnReceiveResponse(std::move(resource_response),
                            std::move(consumer_handle), std::nullopt);
  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = output.size();
  status.encoded_body_length = output.size();
  client->OnComplete(status);
}

}  // namespace content

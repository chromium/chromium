// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/blob_internals_url_loader.h"

#include "content/browser/blob_storage/blob_internals_url_loader.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_response.h"
#include "storage/browser/blob/view_blob_internals_job.h"

namespace content {

void StartBlobInternalsURLLoader(
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    ChromeBlobStorageContext* blob_storage_context) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  network::ResourceResponseHead resource_response;
  resource_response.headers = headers;
  resource_response.mime_type = "text/html";

  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(client_remote));
  client->OnReceiveResponse(resource_response);

  std::string output = storage::ViewBlobInternalsJob::GenerateHTML(
      blob_storage_context->context());
  mojo::DataPipe data_pipe(output.size());

  void* buffer = nullptr;
  uint32_t num_bytes = output.size();
  MojoResult result = data_pipe.producer_handle->BeginWriteData(
      &buffer, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  CHECK_EQ(result, MOJO_RESULT_OK);
  CHECK_EQ(num_bytes, output.size());

  memcpy(buffer, output.c_str(), output.size());
  result = data_pipe.producer_handle->EndWriteData(num_bytes);
  CHECK_EQ(result, MOJO_RESULT_OK);

  client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));
  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = output.size();
  status.encoded_body_length = output.size();
  client->OnComplete(status);
}

}  // namespace content

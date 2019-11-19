// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/background_fetch/background_fetch_types.h"

#include "mojo/public/cpp/bindings/remote.h"

namespace {

blink::mojom::SerializedBlobPtr CloneSerializedBlob(
    const blink::mojom::SerializedBlobPtr& blob) {
  if (blob.is_null())
    return nullptr;
  mojo::Remote<blink::mojom::Blob> blob_remote(std::move(blob->blob));
  blob_remote->Clone(blob->blob.InitWithNewPipeAndPassReceiver());
  return blink::mojom::SerializedBlob::New(blob->uuid, blob->content_type,
                                           blob->size, blob_remote.Unbind());
}

}  // namespace

namespace content {

// static
blink::mojom::FetchAPIResponsePtr BackgroundFetchSettledFetch::CloneResponse(
    const blink::mojom::FetchAPIResponsePtr& response) {
  // TODO(https://crbug.com/876546): Replace this method with response.Clone()
  // if the associated bug is fixed.
  if (response.is_null())
    return nullptr;
  return blink::mojom::FetchAPIResponse::New(
      response->url_list, response->status_code, response->status_text,
      response->response_type, response->response_source, response->headers,
      CloneSerializedBlob(response->blob), response->error,
      response->response_time, response->cache_storage_cache_name,
      response->cors_exposed_header_names,
      CloneSerializedBlob(response->side_data_blob),
      CloneSerializedBlob(response->side_data_blob_for_cache_put),
      response->content_security_policy.Clone());
}

// static
blink::mojom::FetchAPIRequestPtr BackgroundFetchSettledFetch::CloneRequest(
    const blink::mojom::FetchAPIRequestPtr& request) {
  if (request.is_null())
    return nullptr;
  return blink::mojom::FetchAPIRequest::New(
      request->mode, request->is_main_resource_load,
      request->request_context_type, request->frame_type, request->url,
      request->method, request->headers, CloneSerializedBlob(request->blob),
      request->body, request->referrer.Clone(), request->credentials_mode,
      request->cache_mode, request->redirect_mode, request->integrity,
      request->priority, request->fetch_window_id, request->keepalive,
      request->is_reload, request->is_history_navigation);
}

}  // namespace content

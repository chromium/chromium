// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/background_fetch/background_fetch_types.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

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
  // TODO(crbug.com/41409379): Replace this method with response.Clone()
  // if the associated bug is fixed.
  if (response.is_null())
    return nullptr;
  return blink::mojom::FetchAPIResponse::New(
      response->url_list, response->status_code, response->status_text,
      response->response_type, response->padding, response->response_source,
      response->headers, response->mime_type, response->request_method,
      CloneSerializedBlob(response->blob), response->error,
      response->response_time, response->cache_storage_cache_name,
      response->cors_exposed_header_names,
      CloneSerializedBlob(response->side_data_blob),
      CloneSerializedBlob(response->side_data_blob_for_cache_put),
      mojo::Clone(response->parsed_headers), response->connection_info,
      response->alpn_negotiated_protocol, response->was_fetched_via_spdy,
      response->has_range_requested, response->auth_challenge_info,
      response->request_include_credentials);
}

// static
blink::mojom::FetchAPIRequestPtr BackgroundFetchSettledFetch::CloneRequest(
    const blink::mojom::FetchAPIRequestPtr& request) {
  if (request.is_null())
    return nullptr;
  return blink::mojom::FetchAPIRequest::New(
      request->mode, request->is_main_resource_load, request->destination,
      request->frame_type, request->url, request->method, request->headers,
      CloneSerializedBlob(request->blob), request->body,
      request->request_initiator, request->navigation_redirect_chain,
      request->referrer.Clone(), request->credentials_mode, request->cache_mode,
      request->redirect_mode, request->integrity, request->priority,
      request->fetch_window_id, request->keepalive, request->is_reload,
      request->is_history_navigation, request->devtools_stack_id,
      request->trust_token_params.Clone(), request->target_address_space,
      request->attribution_reporting_eligibility,
      request->attribution_reporting_support,
      /*service_worker_race_network_request_token=*/std::nullopt);
}

}  // namespace content

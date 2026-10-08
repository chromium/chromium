// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/background_fetch/background_fetch_types.h"

#include <optional>

#include "base/check.h"
#include "base/check_op.h"
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

  blink::mojom::FetchAPIResponsePtr clone =
      blink::mojom::FetchAPIResponse::New();
  // Copy the fields BGF actually uses; CHECK that the others are not used and
  // don't need copying.
  clone->url_list = response->url_list;
  clone->status_code = response->status_code;
  clone->status_text = response->status_text;
  clone->response_type = response->response_type;
  CHECK_EQ(clone->padding, response->padding);
  CHECK_EQ(clone->response_source, response->response_source);
  clone->headers = response->headers;
  CHECK(!response->mime_type);
  CHECK(!response->request_method);
  clone->blob = CloneSerializedBlob(response->blob);
  CHECK_EQ(clone->error, response->error);
  clone->response_time = response->response_time;
  CHECK(!response->cache_storage_cache_name);
  CHECK(!response->cache_storage_side_data_writer);
  CHECK(response->cors_exposed_header_names.empty());
  CHECK(!response->side_data_blob);
  CHECK(!response->side_data_for_cache_put);
  CHECK(!response->parsed_headers);
  CHECK_EQ(clone->connection_info, response->connection_info);
  CHECK_EQ(clone->alpn_negotiated_protocol, response->alpn_negotiated_protocol);
  CHECK_EQ(clone->was_fetched_via_spdy, response->was_fetched_via_spdy);
  CHECK_EQ(clone->has_range_requested, response->has_range_requested);
  CHECK(!response->auth_challenge_info);
  CHECK_EQ(clone->request_include_credentials,
           response->request_include_credentials);
  CHECK_EQ(clone->timing_allow_passed, response->timing_allow_passed);
  return clone;
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
      /*service_worker_race_network_request_token=*/std::nullopt);
}

}  // namespace content

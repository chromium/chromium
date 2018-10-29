// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_loader_helpers.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "content/public/common/content_features.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_util.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/resource_response.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "ui/base/page_transition_types.h"

namespace content {
namespace {

// Calls |callback| when Blob reading is complete.
class BlobCompleteCaller : public blink::mojom::BlobReaderClient {
 public:
  using BlobCompleteCallback = base::OnceCallback<void(int net_error)>;

  explicit BlobCompleteCaller(BlobCompleteCallback callback)
      : callback_(std::move(callback)) {}
  ~BlobCompleteCaller() override = default;

  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {}
  void OnComplete(int32_t status, uint64_t data_length) override {
    std::move(callback_).Run(base::checked_cast<int>(status));
  }

 private:
  BlobCompleteCallback callback_;
};

}  // namespace

// static
void ServiceWorkerLoaderHelpers::SaveResponseHeaders(
    const int status_code,
    const std::string& status_text,
    const base::flat_map<std::string, std::string>& headers,
    network::ResourceResponseHead* out_head) {
  // Build a string instead of using HttpResponseHeaders::AddHeader on
  // each header, since AddHeader has O(n^2) performance.
  std::string buf(base::StringPrintf("HTTP/1.1 %d %s\r\n", status_code,
                                     status_text.c_str()));
  for (const auto& item : headers) {
    buf.append(item.first);
    buf.append(": ");
    buf.append(item.second);
    buf.append("\r\n");
  }
  buf.append("\r\n");

  out_head->headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(buf.c_str(), buf.size()));

  // Populate |out_head|'s MIME type with the value from the HTTP response
  // headers.
  if (out_head->mime_type.empty()) {
    std::string mime_type;
    if (out_head->headers->GetMimeType(&mime_type))
      out_head->mime_type = mime_type;
  }

  // Populate |out_head|'s charset with the value from the HTTP response
  // headers.
  if (out_head->charset.empty()) {
    std::string charset;
    if (out_head->headers->GetCharset(&charset))
      out_head->charset = charset;
  }

  // Populate |out_head|'s content length with the value from the HTTP response
  // headers.
  if (out_head->content_length == -1)
    out_head->content_length = out_head->headers->GetContentLength();
}

// static
void ServiceWorkerLoaderHelpers::SaveResponseInfo(
    const blink::mojom::FetchAPIResponse& response,
    network::ResourceResponseHead* out_head) {
  out_head->was_fetched_via_service_worker = true;
  out_head->was_fallback_required_by_service_worker = false;
  out_head->url_list_via_service_worker = response.url_list;
  out_head->response_type = response.response_type;
  out_head->response_time = response.response_time;
  out_head->is_in_cache_storage = response.is_in_cache_storage;
  if (response.cache_storage_cache_name)
    out_head->cache_storage_cache_name = *(response.cache_storage_cache_name);
  else
    out_head->cache_storage_cache_name.clear();
  out_head->cors_exposed_header_names = response.cors_exposed_header_names;
  out_head->did_service_worker_navigation_preload = false;
}

// static
base::Optional<net::RedirectInfo>
ServiceWorkerLoaderHelpers::ComputeRedirectInfo(
    const network::ResourceRequest& original_request,
    const network::ResourceResponseHead& response_head) {
  std::string new_location;
  if (!response_head.headers->IsRedirect(&new_location))
    return base::nullopt;

  // If the request is a MAIN_FRAME request, the first-party URL gets
  // updated on redirects.
  const net::URLRequest::FirstPartyURLPolicy first_party_url_policy =
      original_request.resource_type == RESOURCE_TYPE_MAIN_FRAME
          ? net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT
          : net::URLRequest::NEVER_CHANGE_FIRST_PARTY_URL;
  return net::RedirectInfo::ComputeRedirectInfo(
      original_request.method, original_request.url,
      original_request.site_for_cookies, first_party_url_policy,
      original_request.referrer_policy,
      network::ComputeReferrer(original_request.referrer),
      response_head.headers.get(), response_head.headers->response_code(),
      original_request.url.Resolve(new_location), false);
}

int ServiceWorkerLoaderHelpers::ReadBlobResponseBody(
    blink::mojom::BlobPtr* blob,
    uint64_t blob_size,
    base::OnceCallback<void(int)> on_blob_read_complete,
    mojo::ScopedDataPipeConsumerHandle* handle_out) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = blink::BlobUtils::GetDataPipeCapacity(blob_size);

  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoResult rv = mojo::CreateDataPipe(&options, &producer_handle, handle_out);
  if (rv != MOJO_RESULT_OK)
    return net::ERR_FAILED;

  blink::mojom::BlobReaderClientPtr blob_reader_client;
  mojo::MakeStrongBinding(
      std::make_unique<BlobCompleteCaller>(std::move(on_blob_read_complete)),
      mojo::MakeRequest(&blob_reader_client));

  (*blob)->ReadAll(std::move(producer_handle), std::move(blob_reader_client));
  return net::OK;
}

}  // namespace content

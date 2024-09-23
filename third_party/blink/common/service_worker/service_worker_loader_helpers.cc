// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_loader_helpers.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "ui/base/page_transition_types.h"

namespace blink {
namespace {

// Calls |callback| when Blob reading is complete.
class BlobCompleteCaller : public mojom::BlobReaderClient {
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

void SaveResponseHeaders(const mojom::FetchAPIResponse& response,
                         network::mojom::URLResponseHead* out_head) {
  // Build a string instead of using HttpResponseHeaders::AddHeader on
  // each header, since AddHeader has O(n^2) performance.
  std::string buf(base::StringPrintf("HTTP/1.1 %d %s\r\n", response.status_code,
                                     response.status_text.c_str()));
  for (const auto& item : response.headers) {
    buf.append(item.first);
    buf.append(": ");
    buf.append(item.second);
    buf.append("\r\n");
  }
  buf.append("\r\n");

  out_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(buf));

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

  // Populate |out_head|'s encoded data length by checking the response source.
  // If the response is not from network, we store 0 since no data is
  // transferred over network.
  // This aligns with the behavior of when SW does not intercept, and the
  // response is from HTTP cache. In non-SW paths, |encoded_data_length| is
  // updated inside |URLLoader::BuildResponseHead()| using
  // |net::URLRequest::GetTotalReceivedBytes()|. This method returns total
  // amount of data received from network after SSL decoding and proxy handling,
  // and returns 0 when no data is received from network.
  if (out_head->encoded_data_length == -1) {
    out_head->encoded_data_length =
        response.response_source ==
                network::mojom::FetchResponseSource::kNetwork
            ? out_head->headers->GetContentLength()
            : 0;
  }
}

}  // namespace

// static
void ServiceWorkerLoaderHelpers::SaveResponseInfo(
    const mojom::FetchAPIResponse& response,
    network::mojom::URLResponseHead* out_head) {
  out_head->was_fetched_via_service_worker = true;
  out_head->url_list_via_service_worker = response.url_list;
  out_head->response_type = response.response_type;
  out_head->padding = response.padding;
  if (response.mime_type.has_value()) {
    std::string charset;
    bool had_charset = false;
    // The mime type set on |response| may have a charset included.  The
    // loading stack, however, expects the charset to already have been
    // stripped.  Parse out the mime type essence without any charset and
    // store the result on |out_head|.
    net::HttpUtil::ParseContentType(response.mime_type.value(),
                                    &out_head->mime_type, &charset,
                                    &had_charset, nullptr);
  }
  out_head->response_time = response.response_time;
  out_head->service_worker_response_source = response.response_source;
  out_head->cache_storage_cache_name =
      response.cache_storage_cache_name.value_or(std::string());
  out_head->cors_exposed_header_names = response.cors_exposed_header_names;
  out_head->did_service_worker_navigation_preload = false;
  out_head->parsed_headers = mojo::Clone(response.parsed_headers);
  out_head->connection_info = response.connection_info;
  out_head->alpn_negotiated_protocol = response.alpn_negotiated_protocol;
  out_head->was_fetched_via_spdy = response.was_fetched_via_spdy;
  out_head->has_range_requested = response.has_range_requested;
  out_head->auth_challenge_info = response.auth_challenge_info;
  SaveResponseHeaders(response, out_head);
}

// static
std::optional<net::RedirectInfo>
ServiceWorkerLoaderHelpers::ComputeRedirectInfo(
    const network::ResourceRequest& original_request,
    const network::mojom::URLResponseHead& response_head) {
  std::string new_location;
  if (!response_head.headers->IsRedirect(&new_location))
    return std::nullopt;

  // If the request is a MAIN_FRAME request, the first-party URL gets
  // updated on redirects.
  const net::RedirectInfo::FirstPartyURLPolicy first_party_url_policy =
      original_request.destination ==
              network::mojom::RequestDestination::kDocument
          ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
          : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL;
  return net::RedirectInfo::ComputeRedirectInfo(
      original_request.method, original_request.url,
      original_request.site_for_cookies, first_party_url_policy,
      original_request.referrer_policy,
      original_request.referrer.GetAsReferrer().spec(),
      response_head.headers->response_code(),
      original_request.url.Resolve(new_location),
      net::RedirectUtil::GetReferrerPolicyHeader(response_head.headers.get()),
      false /* insecure_scheme_was_upgraded */);
}

int ServiceWorkerLoaderHelpers::ReadBlobResponseBody(
    mojo::Remote<mojom::Blob>* blob,
    uint64_t blob_size,
    base::OnceCallback<void(int)> on_blob_read_complete,
    mojo::ScopedDataPipeConsumerHandle* handle_out) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = BlobUtils::GetDataPipeCapacity(blob_size);

  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoResult rv = mojo::CreateDataPipe(&options, producer_handle, *handle_out);
  if (rv != MOJO_RESULT_OK)
    return net::ERR_FAILED;

  mojo::PendingRemote<mojom::BlobReaderClient> blob_reader_client;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<BlobCompleteCaller>(std::move(on_blob_read_complete)),
      blob_reader_client.InitWithNewPipeAndPassReceiver());

  (*blob)->ReadAll(std::move(producer_handle), std::move(blob_reader_client));
  return net::OK;
}

// static
bool ServiceWorkerLoaderHelpers::IsMainRequestDestination(
    network::mojom::RequestDestination destination) {
  // When PlzDedicatedWorker is enabled, a dedicated worker script is considered
  // to be a main resource.
  if (destination == network::mojom::RequestDestination::kWorker)
    return base::FeatureList::IsEnabled(features::kPlzDedicatedWorker);
  return IsRequestDestinationFrame(destination) ||
         destination == network::mojom::RequestDestination::kSharedWorker;
}

// static
const char* ServiceWorkerLoaderHelpers::FetchResponseSourceToSuffix(
    network::mojom::FetchResponseSource source) {
  // Don't change these returned strings. They are used for recording UMAs.
  switch (source) {
    case network::mojom::FetchResponseSource::kUnspecified:
      return "Unspecified";
    case network::mojom::FetchResponseSource::kNetwork:
      return "Network";
    case network::mojom::FetchResponseSource::kHttpCache:
      return "HttpCache";
    case network::mojom::FetchResponseSource::kCacheStorage:
      return "CacheStorage";
  }
  NOTREACHED_IN_MIGRATION();
  return "Unknown";
}

}  // namespace blink

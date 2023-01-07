// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_url_loader_factory.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "components/web_package/web_bundle_utils.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace content {

// TODO(crbug.com/966753): Consider security models, i.e. plausible CORS
// adoption.
class WebBundleURLLoaderFactory::EntryLoader final
    : public network::mojom::URLLoader {
 public:
  EntryLoader(base::WeakPtr<WebBundleURLLoaderFactory> factory,
              mojo::PendingRemote<network::mojom::URLLoaderClient> client,
              const network::ResourceRequest& resource_request,
              int frame_tree_node_id)
      : factory_(std::move(factory)),
        loader_client_(std::move(client)),
        frame_tree_node_id_(frame_tree_node_id) {
    DCHECK(factory_);

    // Parse the Range header if any.
    // Whether range request should be supported or not is discussed here:
    // https://github.com/WICG/webpackage/issues/478
    std::string range_header;
    if (resource_request.headers.GetHeader(net::HttpRequestHeaders::kRange,
                                           &range_header)) {
      std::vector<net::HttpByteRange> ranges;
      if (net::HttpUtil::ParseRangeHeader(range_header, &ranges) &&
          ranges.size() == 1) {  // We don't support multi-range requests.
        byte_range_ = ranges[0];
      }
    }

    factory_->reader()->ReadResponse(
        resource_request, base::BindOnce(&EntryLoader::OnResponseReady,
                                         weak_factory_.GetWeakPtr()));
  }

  EntryLoader(const EntryLoader&) = delete;
  EntryLoader& operator=(const EntryLoader&) = delete;

  ~EntryLoader() override = default;

 private:
  // network::mojom::URLLoader implementation
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  void OnResponseReady(web_package::mojom::BundleResponsePtr response,
                       web_package::mojom::BundleResponseParseErrorPtr error) {
    if (!factory_ || !loader_client_.is_connected())
      return;

    // TODO(crbug.com/990733): For the initial implementation, we allow only
    // net::HTTP_OK, but we should clarify acceptable status code in the spec.
    if (!response || response->response_code != net::HTTP_OK) {
      if (error) {
        web_bundle_utils::LogErrorMessageToConsole(
            frame_tree_node_id_,
            std::string("Failed to read response header of Web Bundle file: ") +
                error->message);
      }
      loader_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INVALID_WEB_BUNDLE));
      return;
    }

    auto response_head = web_package::CreateResourceResponse(response);
    if (byte_range_) {
      if (byte_range_->ComputeBounds(response->payload_length)) {
        response_head->headers->UpdateWithNewRange(
            *byte_range_, response->payload_length,
            true /*replace_status_line*/);
        // Adjust the offset and length to read.
        // Note: This wouldn't work when the exchange is signed and its payload
        // is mi-sha256 encoded. see crbug.com/1001366
        response->payload_offset += byte_range_->first_byte_position();
        response->payload_length = byte_range_->last_byte_position() -
                                   byte_range_->first_byte_position() + 1;
      } else {
        loader_client_->OnComplete(network::URLLoaderCompletionStatus(
            net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
        return;
      }
    }

    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes =
        std::min(base::strict_cast<uint64_t>(
                     network::features::GetDataPipeDefaultAllocationSize()),
                 response->payload_length);

    auto result =
        mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
    loader_client_->OnReceiveResponse(
        std::move(response_head), std::move(consumer_handle), absl::nullopt);
    if (result != MOJO_RESULT_OK) {
      loader_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
      return;
    }

    factory_->reader()->ReadResponseBody(
        std::move(response), std::move(producer_handle),
        base::BindOnce(&EntryLoader::FinishReadingBody,
                       weak_factory_.GetWeakPtr()));
  }

  void FinishReadingBody(net::Error net_error) {
    if (!loader_client_.is_connected())
      return;

    network::URLLoaderCompletionStatus status;
    status.error_code = net_error;
    loader_client_->OnComplete(status);
  }

  base::WeakPtr<WebBundleURLLoaderFactory> factory_;
  mojo::Remote<network::mojom::URLLoaderClient> loader_client_;
  const int frame_tree_node_id_;
  absl::optional<net::HttpByteRange> byte_range_;

  base::WeakPtrFactory<EntryLoader> weak_factory_{this};
};

WebBundleURLLoaderFactory::WebBundleURLLoaderFactory(
    scoped_refptr<WebBundleReader> reader,
    int frame_tree_node_id)
    : reader_(std::move(reader)), frame_tree_node_id_(frame_tree_node_id) {}

WebBundleURLLoaderFactory::~WebBundleURLLoaderFactory() = default;

void WebBundleURLLoaderFactory::SetFallbackFactory(
    mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory) {
  fallback_factory_ = std::move(fallback_factory);
}

void WebBundleURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (CanHandleRequest(resource_request)) {
    auto loader = std::make_unique<EntryLoader>(
        weak_factory_.GetWeakPtr(), std::move(loader_client), resource_request,
        frame_tree_node_id_);
    std::unique_ptr<network::mojom::URLLoader> url_loader = std::move(loader);
    mojo::MakeSelfOwnedReceiver(
        std::move(url_loader), mojo::PendingReceiver<network::mojom::URLLoader>(
                                   std::move(loader_receiver)));
  } else if (fallback_factory_) {
    fallback_factory_->CreateLoaderAndStart(
        std::move(loader_receiver), request_id, options, resource_request,
        std::move(loader_client), traffic_annotation);
  } else {
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(loader_client))
        ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
  }
}

void WebBundleURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

bool WebBundleURLLoaderFactory::CanHandleRequest(
    const network::ResourceRequest& resource_request) const {
  if (!base::EqualsCaseInsensitiveASCII(resource_request.method,
                                        net::HttpRequestHeaders::kGetMethod)) {
    return false;
  }
  if (reader_->source().is_network() &&
      !reader_->source().IsPathRestrictionSatisfied(resource_request.url)) {
    // For network served bundle, subresources are loaded from the bundle only
    // when their URLs are inside the bundle's scope.
    // https://github.com/WICG/webpackage/blob/master/explainers/navigation-to-unsigned-bundles.md#loading-an-authoritative-unsigned-bundle
    // 'Subsequent subresource requests will also only be served from the bundle
    // if they're inside its scope.'
    return false;
  }
  return reader_->HasEntry(resource_request.url);
}

}  // namespace content

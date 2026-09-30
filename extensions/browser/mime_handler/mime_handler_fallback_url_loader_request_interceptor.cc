// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_fallback_url_loader_request_interceptor.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace extensions::mime_handler {

namespace {

// Delivers one kept response and then does nothing. Lives as long as the
// URLLoader pipe.
class MemoryURLLoader final : public network::mojom::URLLoader {
 public:
  MemoryURLLoader(mojo::PendingRemote<network::mojom::URLLoaderClient> client,
                  MimeHandlerStreamManager::CachedFallbackBody response)
      : client_(std::move(client)) {
    network::URLLoaderCompletionStatus status(net::OK);
    // `decoded_body_length` is the post-decoding byte count from the
    // cache (not `response.head->content_length`, which is the wire
    // `Content-Length` and would be wrong for content-encoded
    // responses).
    status.decoded_body_length = base::ByteSize(response.decoded_body_size);
    if (response.head->content_length >= 0) {
      status.encoded_body_length =
          base::ByteSize(base::as_unsigned(response.head->content_length));
    }
    if (response.head->encoded_data_length >= 0) {
      status.encoded_data_length =
          base::ByteSize(base::as_unsigned(response.head->encoded_data_length));
    }

    client_->OnReceiveResponse(std::move(response.head),
                               std::move(response.pipe),
                               /*cached_metadata=*/std::nullopt);
    client_->OnComplete(status);
  }

  MemoryURLLoader(const MemoryURLLoader&) = delete;
  MemoryURLLoader& operator=(const MemoryURLLoader&) = delete;
  ~MemoryURLLoader() override = default;

  // network::mojom::URLLoader:
  void FollowRedirect(network::HttpRequestHeadersUpdateParams,
                      const std::optional<GURL>&) override {}
  void SetPriority(net::RequestPriority, int32_t) override {}

 private:
  mojo::Remote<network::mojom::URLLoaderClient> client_;
};

void ServeFromMemory(
    MimeHandlerStreamManager::CachedFallbackBody response,
    const network::ResourceRequest&,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MemoryURLLoader>(std::move(client), std::move(response)),
      std::move(receiver));
}

}  // namespace

MimeHandlerFallbackURLLoaderRequestInterceptor::
    MimeHandlerFallbackURLLoaderRequestInterceptor(
        content::FrameTreeNodeId frame_tree_node_id,
        int64_t navigation_id)
    : frame_tree_node_id_(frame_tree_node_id), navigation_id_(navigation_id) {}

MimeHandlerFallbackURLLoaderRequestInterceptor::
    ~MimeHandlerFallbackURLLoaderRequestInterceptor() = default;

void MimeHandlerFallbackURLLoaderRequestInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    LoaderCallback callback) {
  std::move(callback).Run(CreateRequestHandler(tentative_resource_request));
}

content::URLLoaderRequestInterceptor::RequestHandler
MimeHandlerFallbackURLLoaderRequestInterceptor::CreateRequestHandler(
    const network::ResourceRequest& tentative_resource_request) {
  content::WebContents* contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  if (!contents) {
    return {};
  }
  auto* manager = MimeHandlerStreamManager::FromWebContents(contents);
  if (!manager) {
    return {};
  }
  std::optional<MimeHandlerStreamManager::CachedFallbackBody> response =
      manager->TakeCachedFallbackBody(frame_tree_node_id_, navigation_id_,
                                      tentative_resource_request.url);
  if (!response) {
    return {};
  }
  return base::BindOnce(&ServeFromMemory, std::move(*response));
}

}  // namespace extensions::mime_handler

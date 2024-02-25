// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/subresource_proxying_url_loader.h"

#include "content/browser/browsing_topics/browsing_topics_url_loader_interceptor.h"
#include "content/browser/interest_group/ad_auction_url_loader_interceptor.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"

namespace content {

SubresourceProxyingURLLoader::SubresourceProxyingURLLoader(
    WeakDocumentPtr document,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory)
    : resource_request_(resource_request),
      forwarding_client_(std::move(client)) {
  DCHECK(network_loader_factory);

  CHECK(resource_request_.browsing_topics ||
        resource_request_.ad_auction_headers);

  if (resource_request_.browsing_topics) {
    interceptors_.push_back(
        std::make_unique<BrowsingTopicsURLLoaderInterceptor>(
            document, resource_request_));
  }

  if (resource_request_.ad_auction_headers) {
    interceptors_.push_back(std::make_unique<AdAuctionURLLoaderInterceptor>(
        document, resource_request_));
  }

  // Make a copy of `resource_request`, because we may need to modify the
  // request.
  network::ResourceRequest new_resource_request = resource_request;
  for (auto& interceptor : interceptors_) {
    interceptor->WillStartRequest(new_resource_request.headers);
  }

  network_loader_factory->CreateLoaderAndStart(
      loader_.BindNewPipeAndPassReceiver(), request_id, options,
      new_resource_request, client_receiver_.BindNewPipeAndPassRemote(),
      traffic_annotation);

  client_receiver_.set_disconnect_handler(
      base::BindOnce(&SubresourceProxyingURLLoader::OnNetworkConnectionError,
                     base::Unretained(this)));
}

SubresourceProxyingURLLoader::~SubresourceProxyingURLLoader() = default;

void SubresourceProxyingURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  std::vector<std::string> new_removed_headers = removed_headers;
  net::HttpRequestHeaders new_modified_headers = modified_headers;

  for (auto& interceptor : interceptors_) {
    interceptor->WillFollowRedirect(new_url, new_removed_headers,
                                    new_modified_headers);
  }

  loader_->FollowRedirect(new_removed_headers, new_modified_headers,
                          modified_cors_exempt_headers, new_url);
}

void SubresourceProxyingURLLoader::SetPriority(net::RequestPriority priority,
                                               int intra_priority_value) {
  loader_->SetPriority(priority, intra_priority_value);
}

void SubresourceProxyingURLLoader::PauseReadingBodyFromNet() {
  loader_->PauseReadingBodyFromNet();
}

void SubresourceProxyingURLLoader::ResumeReadingBodyFromNet() {
  loader_->ResumeReadingBodyFromNet();
}

void SubresourceProxyingURLLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  forwarding_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void SubresourceProxyingURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  for (auto& interceptor : interceptors_) {
    interceptor->OnReceiveResponse(head);
  }

  forwarding_client_->OnReceiveResponse(std::move(head), std::move(body),
                                        std::move(cached_metadata));
}

void SubresourceProxyingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  for (auto& interceptor : interceptors_) {
    interceptor->OnReceiveRedirect(redirect_info, head);
  }

  forwarding_client_->OnReceiveRedirect(redirect_info, std::move(head));
}

void SubresourceProxyingURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    base::OnceCallback<void()> callback) {
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(callback));
}

void SubresourceProxyingURLLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void SubresourceProxyingURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  forwarding_client_->OnComplete(status);
}

void SubresourceProxyingURLLoader::OnNetworkConnectionError() {
  // The network loader has an error; we should let the client know it's closed
  // by dropping this, which will in turn make this loader destroyed.
  forwarding_client_.reset();
}

}  // namespace content

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/browsing_topics_url_loader.h"

#include "base/bind.h"
#include "content/browser/browsing_topics/header_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/page.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"

namespace content {

namespace {

bool GetTopicsHeaderValueForSubresourceRequest(
    WeakDocumentPtr request_initiator_document,
    const GURL& url,
    std::string& header_value) {
  DCHECK(header_value.empty());

  // Due to the race between the subresource requests and navigations, this
  // request may arrive before the commit confirmation is received (i.e.
  // NavigationRequest::DidCommitNavigation()), or after the document is
  // destroyed. We consider those cases to be ineligible for topics.
  //
  // TODO(yaoxia): measure how often this happens.
  RenderFrameHost* request_initiator_frame =
      request_initiator_document.AsRenderFrameHostIfValid();
  if (!request_initiator_frame)
    return false;

  // Fenced frames disallow most permissions policies which would let this
  // function return false regardless, but adding this check to be more
  // explicit.
  if (request_initiator_frame->IsNestedWithinFencedFrame())
    return false;

  if (!request_initiator_frame->GetPage().IsPrimary())
    return false;

  // TODO(crbug.com/1244137): IsPrimary() doesn't actually detect portals yet.
  // Remove this when it does.
  if (!static_cast<RenderFrameHostImpl*>(
           request_initiator_frame->GetMainFrame())
           ->IsOutermostMainFrame()) {
    return false;
  }

  url::Origin origin = url::Origin::Create(url);
  if (origin.opaque())
    return false;

  // TODO(yaoxia): should this be `ReportBadMessage`? On the renderer side, the
  // fetch initiator context must be secure. Does it imply that the requested
  // `origin` is always potentially trustworthy?
  if (!network::IsOriginPotentiallyTrustworthy(origin))
    return false;

  const blink::PermissionsPolicy* permissions_policy =
      static_cast<RenderFrameHostImpl*>(request_initiator_frame)
          ->permissions_policy();

  if (!permissions_policy->IsFeatureEnabledForOrigin(
          blink::mojom::PermissionsPolicyFeature::kBrowsingTopics, origin) ||
      !permissions_policy->IsFeatureEnabledForOrigin(
          blink::mojom::PermissionsPolicyFeature::
              kBrowsingTopicsBackwardCompatible,
          origin)) {
    return false;
  }

  std::vector<blink::mojom::EpochTopicPtr> topics;
  bool topics_eligible = GetContentClient()->browser()->HandleTopicsWebApi(
      origin, request_initiator_frame->GetMainFrame(),
      browsing_topics::ApiCallerSource::kFetch,
      /*get_topics=*/true,
      /*observe=*/false, topics);

  if (topics_eligible)
    header_value = DeriveTopicsHeaderValue(topics);

  return topics_eligible;
}

void ProcessResponseHeaders(const net::HttpResponseHeaders* response_headers,
                            WeakDocumentPtr document,
                            const GURL& url) {
  if (!response_headers)
    return;

  RenderFrameHost* rfh = document.AsRenderFrameHostIfValid();
  if (!rfh)
    return;

  HandleTopicsEligibleResponse(*response_headers, url::Origin::Create(url),
                               *rfh, browsing_topics::ApiCallerSource::kFetch);
}

}  // namespace

BrowsingTopicsURLLoader::BrowsingTopicsURLLoader(
    WeakDocumentPtr document,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory)
    : document_(std::move(document)),
      url_(resource_request.url),
      forwarding_client_(std::move(client)) {
  DCHECK(network_loader_factory);

  network::ResourceRequest new_resource_request = resource_request;

  std::string header_value;
  topics_eligible_ = GetTopicsHeaderValueForSubresourceRequest(
      document_, new_resource_request.url, header_value);

  if (topics_eligible_) {
    new_resource_request.headers.SetHeader(kBrowsingTopicsRequestHeaderKey,
                                           header_value);
  }

  network_loader_factory->CreateLoaderAndStart(
      loader_.BindNewPipeAndPassReceiver(), request_id, options,
      new_resource_request, client_receiver_.BindNewPipeAndPassRemote(),
      traffic_annotation);

  client_receiver_.set_disconnect_handler(
      base::BindOnce(&BrowsingTopicsURLLoader::OnNetworkConnectionError,
                     base::Unretained(this)));
}

BrowsingTopicsURLLoader::~BrowsingTopicsURLLoader() = default;

void BrowsingTopicsURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  if (new_url)
    url_ = new_url.value();

  std::vector<std::string> new_removed_headers = removed_headers;
  net::HttpRequestHeaders new_modified_headers = modified_headers;

  new_removed_headers.push_back(kBrowsingTopicsRequestHeaderKey);

  std::string header_value;
  topics_eligible_ =
      GetTopicsHeaderValueForSubresourceRequest(document_, url_, header_value);

  if (topics_eligible_) {
    new_modified_headers.SetHeader(kBrowsingTopicsRequestHeaderKey,
                                   header_value);
  }

  loader_->FollowRedirect(new_removed_headers, new_modified_headers,
                          modified_cors_exempt_headers, new_url);
}

void BrowsingTopicsURLLoader::SetPriority(net::RequestPriority priority,
                                          int intra_priority_value) {
  loader_->SetPriority(priority, intra_priority_value);
}

void BrowsingTopicsURLLoader::PauseReadingBodyFromNet() {
  loader_->PauseReadingBodyFromNet();
}

void BrowsingTopicsURLLoader::ResumeReadingBodyFromNet() {
  loader_->ResumeReadingBodyFromNet();
}

void BrowsingTopicsURLLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  forwarding_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void BrowsingTopicsURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  if (topics_eligible_) {
    ProcessResponseHeaders(head->headers.get(), document_, url_);
    topics_eligible_ = false;
  }

  forwarding_client_->OnReceiveResponse(std::move(head), std::move(body),
                                        std::move(cached_metadata));
}

void BrowsingTopicsURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  if (topics_eligible_) {
    ProcessResponseHeaders(head->headers.get(), document_, url_);
    topics_eligible_ = false;
  }

  url_ = redirect_info.new_url;

  forwarding_client_->OnReceiveRedirect(redirect_info, std::move(head));
}

void BrowsingTopicsURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    base::OnceCallback<void()> callback) {
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(callback));
}

void BrowsingTopicsURLLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void BrowsingTopicsURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  forwarding_client_->OnComplete(status);
}

void BrowsingTopicsURLLoader::OnNetworkConnectionError() {
  // The network loader has an error; we should let the client know it's closed
  // by dropping this, which will in turn make this loader destroyed.
  forwarding_client_.reset();
}

}  // namespace content

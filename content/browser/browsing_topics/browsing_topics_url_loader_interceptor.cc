// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/browsing_topics_url_loader_interceptor.h"

#include "content/browser/browsing_topics/header_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/page.h"
#include "content/public/common/content_client.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"

namespace content {

namespace {

void RecordDocumentPresentUma(bool present) {
  base::UmaHistogramBoolean(
      "BrowsingTopics.InterceptedTopicsFetchRequest.DocumentPresent", present);
}

}  // namespace

BrowsingTopicsURLLoaderInterceptor::BrowsingTopicsURLLoaderInterceptor(
    WeakDocumentPtr document,
    const network::ResourceRequest& resource_request)
    : document_(document),
      resource_request_(resource_request),
      url_(resource_request.url) {
  CHECK(resource_request_->browsing_topics);
}

BrowsingTopicsURLLoaderInterceptor::~BrowsingTopicsURLLoaderInterceptor() =
    default;

void BrowsingTopicsURLLoaderInterceptor::WillStartRequest(
    net::HttpRequestHeaders& headers) {
  PopulateRequestOrRedirectHeaders(headers,
                                   /*removed_headers=*/nullptr);
}

void BrowsingTopicsURLLoaderInterceptor::WillFollowRedirect(
    const absl::optional<GURL>& new_url,
    std::vector<std::string>& removed_headers,
    net::HttpRequestHeaders& modified_headers) {
  if (new_url) {
    url_ = new_url.value();
  }

  PopulateRequestOrRedirectHeaders(modified_headers, &removed_headers);
}

void BrowsingTopicsURLLoaderInterceptor::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr& head) {
  ProcessRedirectOrResponseHeaders(head);

  url_ = redirect_info.new_url;
}

void BrowsingTopicsURLLoaderInterceptor::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr& head) {
  ProcessRedirectOrResponseHeaders(head);
}

void BrowsingTopicsURLLoaderInterceptor::PopulateRequestOrRedirectHeaders(
    net::HttpRequestHeaders& headers,
    std::vector<std::string>* removed_headers) {
  topics_eligible_ = false;

  if (removed_headers) {
    removed_headers->push_back(kBrowsingTopicsRequestHeaderKey);
  }

  // Due to the race between the subresource requests and navigations, this
  // request may arrive before the commit confirmation is received (i.e.
  // NavigationRequest::DidCommitNavigation()), or after the document is
  // destroyed. We consider those cases to be ineligible for topics.
  RenderFrameHost* request_initiator_frame =
      document_.AsRenderFrameHostIfValid();

  RecordDocumentPresentUma(request_initiator_frame);

  if (!request_initiator_frame) {
    return;
  }

  // Fenced frames disallow most permissions policies which would let this
  // function return false regardless, but adding this check to be more
  // explicit.
  if (request_initiator_frame->IsNestedWithinFencedFrame()) {
    return;
  }

  if (!request_initiator_frame->GetPage().IsPrimary()) {
    return;
  }

  // TODO(crbug.com/1244137): IsPrimary() doesn't actually detect portals yet.
  // Remove this when it does.
  if (!static_cast<RenderFrameHostImpl*>(
           request_initiator_frame->GetMainFrame())
           ->IsOutermostMainFrame()) {
    return;
  }

  url::Origin origin = url::Origin::Create(url_);
  if (origin.opaque()) {
    return;
  }

  // TODO(yaoxia): should this be `ReportBadMessage`? On the renderer side, the
  // fetch initiator context must be secure. Does it imply that the requested
  // `origin` is always potentially trustworthy?
  if (!network::IsOriginPotentiallyTrustworthy(origin)) {
    return;
  }

  const blink::PermissionsPolicy* permissions_policy =
      static_cast<RenderFrameHostImpl*>(request_initiator_frame)
          ->permissions_policy();

  if (!permissions_policy->IsFeatureEnabledForSubresourceRequest(
          blink::mojom::PermissionsPolicyFeature::kBrowsingTopics, origin,
          *resource_request_) ||
      !permissions_policy->IsFeatureEnabledForSubresourceRequest(
          blink::mojom::PermissionsPolicyFeature::
              kBrowsingTopicsBackwardCompatible,
          origin, *resource_request_)) {
    return;
  }

  std::vector<blink::mojom::EpochTopicPtr> topics;
  topics_eligible_ = GetContentClient()->browser()->HandleTopicsWebApi(
      origin, request_initiator_frame->GetMainFrame(),
      browsing_topics::ApiCallerSource::kFetch,
      /*get_topics=*/true,
      /*observe=*/false, topics);

  if (topics_eligible_) {
    int num_versions_in_epochs =
        GetContentClient()->browser()->NumVersionsInTopicsEpochs(
            request_initiator_frame->GetMainFrame());
    headers.SetHeader(kBrowsingTopicsRequestHeaderKey,
                      DeriveTopicsHeaderValue(topics, num_versions_in_epochs));
  }
}

void BrowsingTopicsURLLoaderInterceptor::ProcessRedirectOrResponseHeaders(
    const network::mojom::URLResponseHeadPtr& head) {
  if (topics_eligible_) {
    RenderFrameHost* rfh = document_.AsRenderFrameHostIfValid();
    if (!rfh) {
      return;
    }

    HandleTopicsEligibleResponse(head->parsed_headers,
                                 url::Origin::Create(url_), *rfh,
                                 browsing_topics::ApiCallerSource::kFetch);

    topics_eligible_ = false;
  }
}

}  // namespace content

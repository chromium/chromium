// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_url_loader_interceptor.h"

#include "content/browser/interest_group/ad_auction_page_data.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/common/content_client.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

constexpr char kAdAuctionRequestHeaderKey[] = "Sec-Ad-Auction-Fetch";

}  // namespace

AdAuctionURLLoaderInterceptor::AdAuctionURLLoaderInterceptor(
    WeakDocumentPtr document,
    const network::ResourceRequest& resource_request)
    : document_(document),
      resource_request_(resource_request),
      request_origin_(url::Origin::Create(resource_request.url)) {
  CHECK(resource_request_->ad_auction_headers);
}

AdAuctionURLLoaderInterceptor::~AdAuctionURLLoaderInterceptor() = default;

void AdAuctionURLLoaderInterceptor::WillStartRequest(
    net::HttpRequestHeaders& headers) {
  // Due to the race between the subresource requests and navigations, this
  // request may arrive before the commit confirmation is received (i.e.
  // NavigationRequest::DidCommitNavigation()), or after the document is
  // destroyed. We consider those cases to be ineligible for ad auction headers.
  //
  // TODO(yaoxia): measure how often this happens.
  RenderFrameHost* request_initiator_frame =
      document_.AsRenderFrameHostIfValid();
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

  if (request_origin_.opaque()) {
    return;
  }

  // TODO(yaoxia): should this be `ReportBadMessage`? On the renderer side, the
  // fetch initiator context must be secure. Does it imply that
  // `request_origin_` is always potentially trustworthy?
  if (!network::IsOriginPotentiallyTrustworthy(request_origin_)) {
    return;
  }

  const blink::PermissionsPolicy* permissions_policy =
      static_cast<RenderFrameHostImpl*>(request_initiator_frame)
          ->permissions_policy();

  if (!permissions_policy->IsFeatureEnabledForSubresourceRequest(
          blink::mojom::PermissionsPolicyFeature::kRunAdAuction,
          request_origin_, *resource_request_)) {
    return;
  }

  ad_auction_headers_eligible_ =
      GetContentClient()->browser()->IsInterestGroupAPIAllowed(
          request_initiator_frame,
          ContentBrowserClient::InterestGroupApiOperation::kSell,
          request_initiator_frame->GetMainFrame()->GetLastCommittedOrigin(),
          request_origin_);

  if (ad_auction_headers_eligible_) {
    headers.SetHeader(kAdAuctionRequestHeaderKey, "?1");
  }
}

void AdAuctionURLLoaderInterceptor::WillFollowRedirect(
    const absl::optional<GURL>& new_url,
    std::vector<std::string>& removed_headers,
    net::HttpRequestHeaders& modified_headers) {
  CHECK(has_redirect_);
  removed_headers.push_back(kAdAuctionRequestHeaderKey);
}

void AdAuctionURLLoaderInterceptor::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHeadPtr& head) {
  has_redirect_ = true;
}

void AdAuctionURLLoaderInterceptor::OnReceiveResponse(
    const network::mojom::URLResponseHeadPtr& head) {
  if (has_redirect_ || !ad_auction_headers_eligible_) {
    return;
  }

  RenderFrameHost* rfh = document_.AsRenderFrameHostIfValid();
  if (!rfh) {
    return;
  }

  const net::HttpResponseHeaders* headers = head->headers.get();

  std::string ad_auction_result;
  if (!headers->GetNormalizedHeader("Ad-Auction-Result", &ad_auction_result) ||
      ad_auction_result.size() != 64) {
    return;
  }

  Page& page = rfh->GetPage();

  AdAuctionPageData* ad_auction_page_data =
      PageUserData<AdAuctionPageData>::GetOrCreateForPage(page);

  ad_auction_page_data->AddAuctionResponseWitnessForOrigin(request_origin_,
                                                           ad_auction_result);
}

}  // namespace content

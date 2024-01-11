// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_url_loader_interceptor.h"

#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "content/browser/interest_group/ad_auction_headers_util.h"
#include "content/browser/interest_group/ad_auction_page_data.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

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
  RenderFrameHostImpl* request_initiator_frame =
      static_cast<RenderFrameHostImpl*>(document_.AsRenderFrameHostIfValid());
  if (!request_initiator_frame) {
    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.NetHeaderResponse.StartRequestOutcome",
        AdAuctionHeadersIsEligibleOutcomeForMetrics::kNoInitiatorFrame);
    return;
  }

  if (!IsAdAuctionHeadersEligible(*request_initiator_frame,
                                  *resource_request_)) {
    return;
  }

  ad_auction_headers_eligible_ = true;
  headers.SetHeader(kAdAuctionRequestHeaderKey, "?1");

  // Pre-warm the data-decoder.
  AdAuctionPageData* ad_auction_page_data =
      PageUserData<AdAuctionPageData>::GetOrCreateForPage(
          request_initiator_frame->GetPage());
  ad_auction_page_data->GetDecoderFor(request_origin_)->GetService();
}

void AdAuctionURLLoaderInterceptor::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr& head) {
  ad_auction_headers_eligible_ = false;
  RemoveAdAuctionResponseHeaders(head->headers);
}

void AdAuctionURLLoaderInterceptor::WillFollowRedirect(
    const std::optional<GURL>& new_url,
    std::vector<std::string>& removed_headers,
    net::HttpRequestHeaders& modified_headers) {
  CHECK(!ad_auction_headers_eligible_);
  removed_headers.push_back(kAdAuctionRequestHeaderKey);
}

void AdAuctionURLLoaderInterceptor::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr& head) {
  RenderFrameHost* request_initiator_frame =
      document_.AsRenderFrameHostIfValid();
  if (ad_auction_headers_eligible_ && request_initiator_frame) {
    ProcessAdAuctionResponseHeaders(
        request_origin_, request_initiator_frame->GetPage(), head->headers);
  } else {
    RemoveAdAuctionResponseHeaders(head->headers);
  }
}

}  // namespace content

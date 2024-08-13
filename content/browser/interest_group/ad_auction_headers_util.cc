// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_headers_util.h"

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "base/base64url.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "content/browser/interest_group/ad_auction_page_data.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "url/origin.h"

namespace content {

const char kAdAuctionRequestHeaderKey[] = "Sec-Ad-Auction-Fetch";
const char kAdAuctionResultResponseHeaderKey[] = "Ad-Auction-Result";
const char kAdAuctionSignalsResponseHeaderKey[] = "Ad-Auction-Signals";
const char kAdAuctionAdditionalBidResponseHeaderKey[] =
    "Ad-Auction-Additional-Bid";

namespace {

// Common conditions checked for eligibility in both
//`IsAdAuctionHeadersEligible` and `IsAdAuctionHeadersEligibleForNavigation`.
bool IsAdAuctionHeadersEligibleInternal(Page& page,
                                        RenderFrameHost* render_frame_host,
                                        const url::Origin& top_frame_origin,
                                        const url::Origin& request_origin) {
  if (!page.IsPrimary()) {
    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.NetHeaderResponse.StartRequestOutcome",
        AdAuctionHeadersIsEligibleOutcomeForMetrics::kNotPrimaryPage);
    return false;
  }

  if (request_origin.opaque()) {
    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.NetHeaderResponse.StartRequestOutcome",
        AdAuctionHeadersIsEligibleOutcomeForMetrics::kOpaqueRequestOrigin);
    return false;
  }

  if (!network::IsOriginPotentiallyTrustworthy(request_origin)) {
    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.NetHeaderResponse.StartRequestOutcome",
        AdAuctionHeadersIsEligibleOutcomeForMetrics::
            kNotPotentiallyTrustworthy);
    return false;
  }

  if (!GetContentClient()->browser()->IsInterestGroupAPIAllowed(
          render_frame_host,
          ContentBrowserClient::InterestGroupApiOperation::kSell,
          top_frame_origin, request_origin)) {
    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.NetHeaderResponse.StartRequestOutcome",
        AdAuctionHeadersIsEligibleOutcomeForMetrics::kApiNotAllowed);
    return false;
  }

  base::UmaHistogramEnumeration(
      "Ads.InterestGroup.NetHeaderResponse.StartRequestOutcome",
      AdAuctionHeadersIsEligibleOutcomeForMetrics::kSuccess);
  return true;
}

}  // namespace

bool IsAdAuctionHeadersEligible(
    RenderFrameHostImpl& initiator_rfh,
    const network::ResourceRequest& resource_request) {
  // Fenced frames disallow most permissions policies which would let this
  // function return false regardless, but adding this check to be more
  // explicit.
  if (initiator_rfh.IsNestedWithinFencedFrame()) {
    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.NetHeaderResponse.StartRequestOutcome",
        AdAuctionHeadersIsEligibleOutcomeForMetrics::kInFencedFrame);
    return false;
  }

  const blink::PermissionsPolicy* permissions_policy =
      initiator_rfh.permissions_policy();
  if (!permissions_policy->IsFeatureEnabledForSubresourceRequest(
          blink::mojom::PermissionsPolicyFeature::kRunAdAuction,
          url::Origin::Create(resource_request.url), resource_request)) {
    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.NetHeaderResponse.StartRequestOutcome",
        AdAuctionHeadersIsEligibleOutcomeForMetrics::
            kDisabledByPermissionsPolicy);
    return false;
  }

  return IsAdAuctionHeadersEligibleInternal(
      /*page=*/initiator_rfh.GetPage(),
      /*render_frame_host=*/&initiator_rfh,
      /*top_frame_origin=*/
      initiator_rfh.GetMainFrame()->GetLastCommittedOrigin(),
      /*request_origin=*/url::Origin::Create(resource_request.url));
}

bool IsAdAuctionHeadersEligibleForNavigation(
    const FrameTreeNode& frame,
    const url::Origin& navigation_request_origin) {
  if (!base::FeatureList::IsEnabled(features::kEnableIFrameAdAuctionHeaders)) {
    return false;
  }

  // Fenced frames disallow most permissions policies which would let this
  // function return false regardless, but adding this check to be more
  // explicit.
  if (frame.IsInFencedFrameTree()) {
    return false;
  }

  // The provided frame represents the iframe, so it must have a parent frame.
  if (!frame.GetParentOrOuterDocument()) {
    return false;
  }

  const blink::PermissionsPolicy* parent_policy =
      frame.GetParentOrOuterDocument()->permissions_policy();
  DCHECK(parent_policy);
  if (!parent_policy->IsFeatureEnabledForOrigin(
          blink::mojom::PermissionsPolicyFeature::kRunAdAuction,
          navigation_request_origin)) {
    return false;
  }

  return IsAdAuctionHeadersEligibleInternal(
      /*page=*/frame.current_frame_host()->GetPage(),
      /*render_frame_host=*/frame.GetParentOrOuterDocument(),
      /*top_frame_origin=*/frame.frame_tree().root()->current_origin(),
      /*request_origin=*/navigation_request_origin);
}

// Please note: before modifying this function, please acknowledge this is
// processing untrusted content from a non sandboxed process. So please keep
// this function simple and avoid adding custom logic.
//
// Fuzzer: ad_auction_headers_util_fuzzer
std::vector<std::string> ParseAdAuctionResultResponseHeader(
    const std::string& ad_auction_results) {
  std::vector<std::string> parsed_results;
  for (const auto& result :
       base::SplitString(ad_auction_results, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    std::string result_bytes;
    if (!base::Base64UrlDecode(result,
                               base::Base64UrlDecodePolicy::IGNORE_PADDING,
                               &result_bytes)) {
      continue;
    }
    if (result_bytes.size() != 32) {
      continue;
    }
    parsed_results.emplace_back(std::move(result_bytes));
  }
  return parsed_results;
}

// Please note: before modifying this function, please acknowledge this is
// processing untrusted content from a non sandboxed process. So please keep
// this function simple and avoid adding custom logic.
//
// Fuzzer: ad_auction_headers_util_fuzzer
void ParseAdAuctionAdditionalBidResponseHeader(
    const std::string& header_line,
    std::map<std::string, std::vector<std::string>>&
        nonce_additional_bids_map) {
  // Skip if `header_line` doesn't match the format
  // <36 characters auction nonce>:<base64-encoded signed additional bid>
  std::vector<std::string> nonce_and_additional_bid = base::SplitString(
      header_line, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (nonce_and_additional_bid.size() != 2) {
    return;
  }

  const std::string& nonce = nonce_and_additional_bid[0];
  if (nonce.size() != 36) {
    return;
  }
  const std::string& additional_bid = nonce_and_additional_bid[1];

  nonce_additional_bids_map[nonce].push_back(additional_bid);
}

// NOTE: This function processes untrusted content, in an unsafe language, from
// an unsandboxed process. As such, minimize the amount of parsing done in this
// function, and instead separate it into distinct functions that are covered
// by fuzz tests.
void ProcessAdAuctionResponseHeaders(
    const url::Origin& request_origin,
    Page& page,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  if (!headers) {
    return;
  }
  AdAuctionPageData* ad_auction_page_data =
      PageUserData<AdAuctionPageData>::GetOrCreateForPage(page);

  if (base::FeatureList::IsEnabled(
          blink::features::kFledgeBiddingAndAuctionServer)) {
    std::string ad_auction_results;
    if (headers->GetNormalizedHeader(kAdAuctionResultResponseHeaderKey,
                                     &ad_auction_results)) {
      for (const std::string& parsed_result :
           ParseAdAuctionResultResponseHeader(ad_auction_results)) {
        ad_auction_page_data->AddAuctionResultWitnessForOrigin(request_origin,
                                                               parsed_result);
      }
    }
  }
  // We intentionally leave the `Ad-Auction-Result` response header in place.

  if (base::FeatureList::IsEnabled(blink::features::kAdAuctionSignals)) {
    std::string ad_auction_signals;
    if (headers->GetNormalizedHeader(kAdAuctionSignalsResponseHeaderKey,
                                     &ad_auction_signals)) {
      if (ad_auction_signals.size() <=
          static_cast<size_t>(
              blink::features::kAdAuctionSignalsMaxSizeBytes.Get())) {
        ad_auction_page_data->AddAuctionSignalsWitnessForOrigin(
            request_origin, ad_auction_signals);
      }
    }
  }
  headers->RemoveHeader(kAdAuctionSignalsResponseHeaderKey);

  std::map<std::string, std::vector<std::string>> nonce_additional_bids_map;
  size_t iter = 0;
  std::string header_line;
  while (headers->EnumerateHeader(
      &iter, kAdAuctionAdditionalBidResponseHeaderKey, &header_line)) {
    ParseAdAuctionAdditionalBidResponseHeader(header_line,
                                              nonce_additional_bids_map);
  }
  if (!nonce_additional_bids_map.empty()) {
    ad_auction_page_data->AddAuctionAdditionalBidsWitnessForOrigin(
        request_origin, nonce_additional_bids_map);
  }
  headers->RemoveHeader(kAdAuctionAdditionalBidResponseHeaderKey);
}

void RemoveAdAuctionResponseHeaders(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  if (!headers) {
    return;
  }
  // We intentionally leave the `Ad-Auction-Result` response header in place.
  headers->RemoveHeader(kAdAuctionSignalsResponseHeaderKey);
  headers->RemoveHeader(kAdAuctionAdditionalBidResponseHeaderKey);
}

}  // namespace content

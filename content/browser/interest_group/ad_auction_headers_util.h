// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_HEADERS_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_HEADERS_UTIL_H_

#include <functional>

#include "base/memory/scoped_refptr.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/weak_document_ptr.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/origin.h"

namespace content {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AdAuctionHeadersIsEligibleOutcomeForMetrics {
  kNoInitiatorFrame = 0,
  kInFencedFrame = 1,
  kNotPrimaryPage = 2,
  // kNotOutermostMainFrame = 3, DEPRECATED
  kOpaqueRequestOrigin = 4,
  kNotPotentiallyTrustworthy = 5,
  kDisabledByPermissionsPolicy = 6,
  kApiNotAllowed = 7,
  kSuccess = 8,
  kMaxValue = kSuccess,
};

// The request header key that triggers interception of the auction result,
// signals, and additional bids from their associated response headers.
extern const char kAdAuctionRequestHeaderKey[];

// Response header keys associated with auction result, signals, and
// additional bids, respectively.
extern const char CONTENT_EXPORT kAdAuctionResultResponseHeaderKey[];
extern const char CONTENT_EXPORT kAdAuctionSignalsResponseHeaderKey[];
extern const char CONTENT_EXPORT kAdAuctionAdditionalBidResponseHeaderKey[];

// Returns whether or not this request is eligible for ad auction headers
// requested for fetch requests. The `initiator_rfh` should be the
// frame from which the fetch request is being issued.`initiator_rfh`
// is not modified by this function, and needs to be passed in non-const only
// because downstream functions use it for console logging.
CONTENT_EXPORT bool IsAdAuctionHeadersEligible(
    RenderFrameHostImpl& initiator_rfh,
    const network::ResourceRequest& resource_request);

// Returns whether or not this request is eligible for ad auction headers
// requested for iframe navigations. The `frame` argument provided should be
// that of the iframe. This uses the parent frame's permissions policy to
// provide greater consistency with fetch requests by treating the iframe
// navigation as a subresource request.
CONTENT_EXPORT bool IsAdAuctionHeadersEligibleForNavigation(
    const FrameTreeNode& frame,
    const url::Origin& navigation_request_origin);

// NOTE: Exposed only for fuzz testing. This is used by
// `ProcessAdAuctionResponseHeaders`, declared below.
//
// Splits and base64 decodes the `Ad-Auction-Result` response header,
// and returns the results. This function processes untrusted content, in an
// unsafe language, from an unsandboxed process, hence the fuzz test coverage.
CONTENT_EXPORT std::vector<std::string> ParseAdAuctionResultResponseHeader(
    const std::string& ad_auction_results);

// NOTE: Exposed only for fuzz testing. This is used by
// `ProcessAdAuctionResponseHeaders`, declared below.
//
// Splits and validates the `Ad-Auction-Additional-Bid` response header,
// and inserts the resulting additional bids into the provided map. This
// function processes untrusted content, in an unsafe language, from an
// unsandboxed process, hence the fuzz test coverage.
CONTENT_EXPORT void ParseAdAuctionAdditionalBidResponseHeader(
    const std::string& header_line,
    std::map<std::string, std::vector<std::string>>& nonce_additional_bids_map);

// Parses and sets the values of the `Ad-Auction-Result`, `Ad-Auction-Signals`,
// and `Ad-Auction-Additional-Bid` headers on the `AdAuctionPageData`
// associated with the `render_frame_host` for later use in the auction.
// Clears the `Ad-Auction-Signals` and `Ad-Auction-Additional-Bid` headers from
// `headers`.
CONTENT_EXPORT void ProcessAdAuctionResponseHeaders(
    const url::Origin& request_origin,
    Page& page,
    scoped_refptr<net::HttpResponseHeaders> headers);

// Removes the `Ad-Auction-Signals` and `Ad-Auction-Additional-Bid` response
// headers from `headers`. Called when the request encountered a redirect or
// error page.
CONTENT_EXPORT void RemoveAdAuctionResponseHeaders(
    scoped_refptr<net::HttpResponseHeaders> headers);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_HEADERS_UTIL_H_

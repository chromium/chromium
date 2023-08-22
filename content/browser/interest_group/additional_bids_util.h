// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BIDS_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BIDS_UTIL_H_

#include <string>

#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "base/values.h"
#include "content/browser/interest_group/interest_group_auction.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

struct CONTENT_EXPORT AdditionalBidDecodeResult {
  AdditionalBidDecodeResult();
  AdditionalBidDecodeResult(const AdditionalBidDecodeResult& other) = delete;
  AdditionalBidDecodeResult(AdditionalBidDecodeResult&& other);
  ~AdditionalBidDecodeResult();

  AdditionalBidDecodeResult& operator=(const AdditionalBidDecodeResult&) =
      delete;
  AdditionalBidDecodeResult& operator=(AdditionalBidDecodeResult&&);

  std::unique_ptr<InterestGroupAuction::BidState> bid_state;
  std::unique_ptr<InterestGroupAuction::Bid> bid;
};

// Tries to parse a "bid" object `bid_in` specified as part of "additionalBids",
// and to construct corresponding InterestGroupAuction::Bid and BidState.
//
// `auction` will only be used to populate the `auction` field of the
// returned result's `bid`, so may be null for tests.
//
// `auction_nonce` is the expected nonce for the bid.
//
// `seller` is expected seller for the auction the bid is to participate in.
//
// `top_level_seller` should be set for the component auctions only, and specify
// the seller of the enclosing top-level auction.
//
// On success, returns an AdditionalBidDecodeResult. Note that `*bid` will
// have a pointer to `*bid_state`.
//
// On failure, returns an error message.
CONTENT_EXPORT base::expected<AdditionalBidDecodeResult, std::string>
DecodeAdditionalBid(InterestGroupAuction* auction,
                    const base::Value& bid_in,
                    const base::Uuid& auction_nonce,
                    const url::Origin& seller,
                    base::optional_ref<const url::Origin> top_level_seller);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BIDS_UTIL_H_

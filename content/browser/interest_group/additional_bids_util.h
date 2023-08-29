// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BIDS_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BIDS_UTIL_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

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

  // `negative_target_joining_origin` is required if there is more than one
  // entry in `negative_target_interest_group_names` (and DecodeAdditionalBid
  // ensures it's set in that case).
  absl::optional<url::Origin> negative_target_joining_origin;
  std::vector<std::string> negative_target_interest_group_names;
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

struct CONTENT_EXPORT SignedAdditionalBid {
  struct Signature {
    std::array<uint8_t, 32> key;
    std::array<uint8_t, 64> signature;
  };

  SignedAdditionalBid();
  SignedAdditionalBid(const SignedAdditionalBid& other) = delete;
  SignedAdditionalBid(SignedAdditionalBid&& other);
  ~SignedAdditionalBid();

  SignedAdditionalBid& operator=(const SignedAdditionalBid&) = delete;
  SignedAdditionalBid& operator=(SignedAdditionalBid&&);

  std::string additional_bid_json;
  std::vector<Signature> signatures;
};

// Tries to decode a signed additional bid JSON represented as
// `signed_additional_bid_in`.
CONTENT_EXPORT base::expected<SignedAdditionalBid, std::string>
DecodeSignedAdditionalBid(base::Value signed_additional_bid_in);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BIDS_UTIL_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BIDS_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BIDS_UTIL_H_

#include <stdint.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "base/values.h"
#include "content/browser/interest_group/auction_metrics_recorder.h"
#include "content/browser/interest_group/interest_group_auction.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
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
  std::optional<url::Origin> negative_target_joining_origin;
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
                    const base::flat_set<url::Origin>& interest_group_buyers,
                    const url::Origin& seller,
                    base::optional_ref<const url::Origin> top_level_seller);

struct CONTENT_EXPORT SignedAdditionalBidSignature {
  blink::InterestGroup::AdditionalBidKey key;
  std::array<uint8_t, 64> signature;
};

struct CONTENT_EXPORT SignedAdditionalBid {
  SignedAdditionalBid();
  SignedAdditionalBid(const SignedAdditionalBid& other) = delete;
  SignedAdditionalBid(SignedAdditionalBid&& other);
  ~SignedAdditionalBid();

  SignedAdditionalBid& operator=(const SignedAdditionalBid&) = delete;
  SignedAdditionalBid& operator=(SignedAdditionalBid&&);

  std::string additional_bid_json;
  std::vector<SignedAdditionalBidSignature> signatures;

  // Returns a vector of indices of signatures that succeed in verifying.
  std::vector<size_t> VerifySignatures();
};

// Tries to decode a signed additional bid JSON represented as
// `signed_additional_bid_in`.
CONTENT_EXPORT base::expected<SignedAdditionalBid, std::string>
DecodeSignedAdditionalBid(base::Value signed_additional_bid_in);

// This class keeps the memory index of negative targeting interest groups
// and helps decide whether they apply to particular bids.
class CONTENT_EXPORT AdAuctionNegativeTargeter {
 public:
  AdAuctionNegativeTargeter();
  ~AdAuctionNegativeTargeter();

  // Register a negative targeting group for buyer `buyer` named `name`
  // that was joined from `joining_origin` with public key `key`.
  void AddInterestGroupInfo(const url::Origin& buyer,
                            const std::string& name,
                            const url::Origin& joining_origin,
                            const blink::InterestGroup::AdditionalBidKey& key);

  // Returns the number of negative interest groups added to this targeter
  // using AddInterestGroupInfo.
  size_t GetNumNegativeInterestGroups();

  // Returns true if negative targeting applies to a bid.
  //
  // `buyer` is the purported origin of the additional bid.
  //
  // `negative_target_joining_origin`, `negative_target_interest_group_names`
  // is the negative targeting info provided by the additional bid.
  //
  // `signatures` are the signatures that were included with the signed
  // additional bid.
  //
  // `valid_signatures` are indices into `signatures` specifying which ones
  // are actually valid signatures for the key. (This does not mean they are
  // valid signatures done by the `buyer`).
  //
  // `seller` is the seller of the auction this is participating in. It's
  // only used for error messages.
  //
  // Any warning messages that may be of interest to developer will be
  // collected into `errors_out`.
  bool ShouldDropDueToNegativeTargeting(
      const url::Origin& buyer,
      const std::optional<url::Origin>& negative_target_joining_origin,
      const std::vector<std::string>& negative_target_interest_group_names,
      const std::vector<SignedAdditionalBidSignature>& signatures,
      const std::vector<size_t>& valid_signatures,
      const url::Origin& seller,
      AuctionMetricsRecorder& auction_metrics_recorder,
      std::vector<std::string>& errors_out);

 private:
  struct NegativeInfo {
    NegativeInfo();
    NegativeInfo(const NegativeInfo& other) = delete;
    NegativeInfo(NegativeInfo&& other) = delete;
    ~NegativeInfo();

    NegativeInfo& operator=(const NegativeInfo&) = delete;
    NegativeInfo& operator=(NegativeInfo&&) = delete;

    url::Origin joining_origin;
    blink::InterestGroup::AdditionalBidKey key;
  };
  std::map<std::pair<url::Origin, std::string>, NegativeInfo>
      negative_interest_groups_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BIDS_UTIL_H_

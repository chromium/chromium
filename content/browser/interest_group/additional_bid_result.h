// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BID_RESULT_H_
#define CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BID_RESULT_H_

namespace content {

// Result of decoding an additional bid. Used for histograms. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class AdditionalBidResult {
  // The additional bid was sent for scoring to participate in the auction.
  kSentForScoring = 0,

  // The additional bid was suppressed because of the presence of at least one
  // of its specified negative targeting interest groups.
  kNegativeTargeted = 1,

  // The additional bid was rejected because the signed additional bid in the
  // Ad-Auction-Additional-Bid response header was not a valid base64 string.
  kRejectedDueToInvalidBase64 = 2,

  // The additional bid was rejected because the signed additional bid was
  // invalid JSON, and so failed to parse.
  kRejectedDueToSignedBidJsonParseError = 3,

  // The additional bid was rejected because the signed additional bid JSON
  // was not structured as expected.
  kRejectedDueToSignedBidDecodeError = 4,

  // The additional bid was rejected because the additional bid from inside
  // the signed additional bid was invalid JSON.
  kRejectedDueToJsonParseError = 5,

  // The additional bid was rejected because the additional bid from inside
  // the signed additional bid was not structured as expected. This is the
  // result used when the replay-prevention fields - auctionNonce, seller and
  // topLevelSeller - was missing or didn't match those of the auction. This
  // is also the result used when the additional bid owner was not found in
  // the auction's interestGroupBuyers.
  kRejectedDueToDecodeError = 6,

  // The additional bid was rejected because the additional bid's owner was
  // not allowed to bid in that auction by IsInterestGroupAPIAllowed.
  kRejectedDueToBuyerNotAllowed = 7,

  // The additional bid was rejected because the additional bid specified a
  // currency that didn't match the currency associated with that buyer.
  kRejectedDueToCurrencyMismatch = 8,

  // Decoding failed because the decoder service is not available due to
  // page being in process of unload.
  kRejectedDecoderShutDown = 9,

  kMaxValue = kRejectedDecoderShutDown
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_ADDITIONAL_BID_RESULT_H_

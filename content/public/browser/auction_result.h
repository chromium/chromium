// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AUCTION_RESULT_H_
#define CONTENT_PUBLIC_BROWSER_AUCTION_RESULT_H_

namespace content {

// Final result of a FLEDGE auction or component auction. This auction could be
// either on-device or on the bidding and auction server. Used for histograms.
// Only recorded for valid auctions. These are used in histograms, so values of
// existing entries must not change when adding/removing values, and obsolete
// values must not be reused.
enum class AuctionResult {
  // The auction succeeded, with a winning bidder.
  kSuccess = 0,

  // The auction was aborted, due to either navigating away from the frame
  // that started the auction or browser shutdown.
  kAborted = 1,

  // Bad message received over Mojo. This is potentially a security error.
  kBadMojoMessage = 2,

  // The user was in no interest groups that could participate in the auction.
  kNoInterestGroups = 3,

  // The seller worklet failed to load.
  kSellerWorkletLoadFailed = 4,

  // The seller worklet crashed.
  kSellerWorkletCrashed = 5,

  // All bidders failed to bid. This happens when all bidders choose not to
  // bid, fail to load, or crash before making a bid.
  kNoBids = 6,

  // The seller worklet rejected all bids (of which there was at least one).
  kAllBidsRejected = 7,

  // Obsolete:
  // kWinningBidderWorkletCrashed = 8,

  // The seller is not allowed to use the interest group API.
  kSellerRejected = 9,

  // The component auction completed with a winner, but that winner lost the
  // top-level auction.
  kComponentLostAuction = 10,

  // Obsolete:
  // kWinningComponentSellerWorkletCrashed = 11,

  // The bidding and auction server response could not be parsed or was
  // invalid.
  kInvalidServerResponse = 12,

  // The auction nonce passed into a call to runAdAuction didn't come from a
  // prior call to createAuctionNonce.
  kInvalidAuctionNonce = 13,

  kMaxValue = kInvalidAuctionNonce
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AUCTION_RESULT_H_

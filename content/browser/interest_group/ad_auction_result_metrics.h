// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_RESULT_METRICS_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_RESULT_METRICS_H_

#include <climits>

#include "base/numerics/clamped_math.h"
#include "base/time/time.h"
#include "content/public/browser/page_user_data.h"

namespace content {

class Page;

// Reports UMA about success / failure auction patterns, and implements a
// feature parameter to control the maximum number of auctions per-page.
class AdAuctionResultMetrics
    : public content::PageUserData<AdAuctionResultMetrics> {
 public:
  enum class AuctionResult { kSucceeded, kFailed };

  explicit AdAuctionResultMetrics(content::Page& page);
  ~AdAuctionResultMetrics() override;

  // To reduce the amount of information that may be exposed to the page from
  // auction outcomes, the number of auctions per page may be limited by a
  // feature parameter.
  //
  // Before starting an auction, this function should be consulted to check if
  // this limit has already been encountered.
  //
  // This function should be called only once per auction attempt, since it
  // modifies internal state.
  bool ShouldRunAuction();

  // After an auction as completed, this function should be called to report
  // whether the auction succeeded or failed.
  //
  // This function should *not* be called for configuration failures where the
  // auction result reveals no information about stored interest groups.
  void ReportAuctionResult(AuctionResult result);

 private:
  // Number of bits to record for the first N auctions, including a leading 1.
  // That is, kNumFirstAuctionBits - 1 auctions will be recorded. Increasing
  // this number will increase the number of UMA buckets, so don't set it too
  // high.
  static constexpr int kNumFirstAuctionBits = 7;

  // The number of calls to ShouldRunAuction(); used for enforcing auction
  // limits.
  base::ClampedNumeric<int> num_requested_auctions_ = 0;

  // The number of auctions that ran to completion, successful and failed --
  // this is also the number of ReportAuctionResult() calls.
  base::ClampedNumeric<int> num_completed_auctions_ = 0;

  // The number of auctions that ran to completion and succeeded.
  base::ClampedNumeric<int> num_successful_auctions_ = 0;

  // The number of auctions requested that didn't run because the page auction
  // limit had been reached. Should equal num_requested_auctions_ -
  // num_completed_auctions_, which is DCHECK'd on destruction.
  base::ClampedNumeric<int> num_auctions_not_run_due_to_auction_limit_ = 0;

  // Stores the bitfield of the first kNumFirstAuctionBits auction results.
  uint8_t first_auction_bits_ = 1u;

  // Stores the time of the last completed auction.
  base::TimeTicks last_auction_time_ = base::TimeTicks::Min();

  static_assert(kNumFirstAuctionBits <=
                    sizeof(AdAuctionResultMetrics::first_auction_bits_) *
                        CHAR_BIT,
                "Not enough bits in `first_auction_bits_`.");

  friend PageUserData;
  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_RESULT_METRICS_H_

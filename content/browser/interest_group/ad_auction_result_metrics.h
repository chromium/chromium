// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_RESULT_METRICS_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_RESULT_METRICS_H_

#include <climits>

#include "base/time/time.h"
#include "content/public/browser/page_user_data.h"

namespace content {

class Page;

class AdAuctionResultMetrics
    : public content::PageUserData<AdAuctionResultMetrics> {
 public:
  enum class AuctionResult { kSucceeded, kFailed };

  explicit AdAuctionResultMetrics(content::Page& page);
  ~AdAuctionResultMetrics() override;

  void ReportAuctionResult(AuctionResult result);

 private:
  // Number of bits to record for the first N auctions, including a leading 1.
  // That is, kNumFirstAuctionBits - 1 auctions will be recorded. Increasing
  // this number will increase the number of UMA buckets, so don't set it too
  // high.
  static constexpr int kNumFirstAuctionBits = 7;

  int num_auctions_ = 0;
  int num_successful_auctions_ = 0;
  uint8_t first_auction_bits_ = 1u;
  base::TimeTicks last_auction_time_ = base::TimeTicks::Min();

  static_assert(kNumFirstAuctionBits <=
                    sizeof(AdAuctionResultMetrics::first_auction_bits_) *
                        CHAR_BIT,
                "Not enough bits in `last_auction_time_`.");

  friend PageUserData;
  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AD_AUCTION_RESULT_METRICS_H_

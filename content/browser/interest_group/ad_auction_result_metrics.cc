// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_result_metrics.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/sparse_histogram.h"
#include "base/time/time.h"
#include "content/public/browser/page_user_data.h"

namespace content {

class Page;

AdAuctionResultMetrics::AdAuctionResultMetrics(content::Page& page)
    : PageUserData<AdAuctionResultMetrics>(page) {}

AdAuctionResultMetrics::~AdAuctionResultMetrics() {
  if (num_auctions_ <= 0)
    return;
  base::UmaHistogramCounts100("Ads.InterestGroup.Auction.NumAuctionsPerPage",
                              num_auctions_);
  base::UmaHistogramPercentage(
      "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage",
      num_successful_auctions_ * 100 / num_auctions_);
  DCHECK_GE(first_auction_bits_, 0b10u);
  DCHECK_LE(first_auction_bits_, (1 << kNumFirstAuctionBits) - 1);
  base::SparseHistogram::FactoryGet(
      "Ads.InterestGroup.Auction.First6AuctionsBitsPerPage",
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(first_auction_bits_);
}

void AdAuctionResultMetrics::ReportAuctionResult(
    AdAuctionResultMetrics::AuctionResult result) {
  num_auctions_++;
  if (num_auctions_ < kNumFirstAuctionBits)
    first_auction_bits_ <<= 1;
  if (result == AdAuctionResultMetrics::AuctionResult::kSucceeded) {
    num_successful_auctions_++;
    if (num_auctions_ < kNumFirstAuctionBits)
      first_auction_bits_ |= 0x1;
  }
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta time_since_last_auction =
      (last_auction_time_ == base::TimeTicks::Min()) ? base::TimeDelta::Max()
                                                     : now - last_auction_time_;
  last_auction_time_ = now;

  base::UmaHistogramLongTimes100(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage",
      time_since_last_auction);
}

PAGE_USER_DATA_KEY_IMPL(AdAuctionResultMetrics);

}  // namespace content

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_result_metrics.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/sparse_histogram.h"
#include "base/time/time.h"
#include "content/common/features.h"
#include "content/public/browser/page_user_data.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class Page;

AdAuctionResultMetrics::AdAuctionResultMetrics(content::Page& page)
    : PageUserData<AdAuctionResultMetrics>(page) {}

AdAuctionResultMetrics::~AdAuctionResultMetrics() {
  if (num_completed_auctions_ <= 0)
    return;
  // Check that every non-skipped auction should complete (but skip the check if
  // clamping may have occurred).
  if (num_requested_auctions_ <
          std::numeric_limits<decltype(num_requested_auctions_)::type>::max() &&
      num_auctions_not_run_due_to_auction_limit_.RawValue() !=
          (num_requested_auctions_ - num_completed_auctions_).RawValue()) {
    // TODO(crbug.com/354735928): Add back removed DCHECK once the
    // "RenderDocument on main frames" feature ships. In the meantime, if we're
    // here, we've encountered racy page destruction, so metrics may be wrong.
    // Don't report metrics in that case.
    return;
  }
  base::UmaHistogramCounts100("Ads.InterestGroup.Auction.NumAuctionsPerPage",
                              num_completed_auctions_);
  base::UmaHistogramPercentage(
      "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage",
      num_successful_auctions_ * 100 / num_completed_auctions_);
  DCHECK_GE(first_auction_bits_, 0b10u);
  DCHECK_LE(first_auction_bits_, (1 << kNumFirstAuctionBits) - 1);
  base::SparseHistogram::FactoryGet(
      "Ads.InterestGroup.Auction.First6AuctionsBitsPerPage",
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(first_auction_bits_);
  base::UmaHistogramCounts100(
      "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit",
      num_auctions_not_run_due_to_auction_limit_);
}

bool AdAuctionResultMetrics::ShouldRunAuction() {
  num_requested_auctions_++;
  if (!base::FeatureList::IsEnabled(features::kFledgeLimitNumAuctions))
    return true;
  if (num_requested_auctions_ > features::kFledgeLimitNumAuctionsParam.Get()) {
    num_auctions_not_run_due_to_auction_limit_++;
    return false;
  }
  return true;
}

void AdAuctionResultMetrics::ReportAuctionResult(
    AdAuctionResultMetrics::AuctionResult result) {
  num_completed_auctions_++;
  if (num_completed_auctions_ < kNumFirstAuctionBits)
    first_auction_bits_ <<= 1;
  if (result == AdAuctionResultMetrics::AuctionResult::kSucceeded) {
    num_successful_auctions_++;
    if (num_completed_auctions_ < kNumFirstAuctionBits)
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

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_METRICS_RECORDER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_METRICS_RECORDER_H_

#include <stdint.h>
#include <set>

#include "base/time/time.h"
#include "content/browser/interest_group/auction_result.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-shared.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

namespace content {

// The AuctionMetricsRecorder is an auction-scoped collection of data used to
// record UKMs used to investigate auction latency used to detect regressive
// trends over time and to drive future optimizations.
//
// The convention here is that:
// - methods prefixed with On are called once, at some milestone in the auction.
// - methods prefixed with Set are called once, whenever that data is available.
// - methods prefixed with Record are called many times, whenever some repeated
//   event happens. The metrics associated with these are set on the builder
//   OnAuctionEnd, just before the Event is written to the UkmRecorder.
class CONTENT_EXPORT AuctionMetricsRecorder {
 public:
  // This object's construction time is that recorded as the auction's start
  // time, for measuring latency end-to-end and LoadInterestGroupPhase latency.
  explicit AuctionMetricsRecorder(ukm::SourceId ukm_source_id);
  ~AuctionMetricsRecorder();

  AuctionMetricsRecorder(const AuctionMetricsRecorder&) = delete;
  AuctionMetricsRecorder& operator=(const AuctionMetricsRecorder&) = delete;

  // Records the auction's end time (for EndToEndLatency) and the AuctionResult.
  // As we're expecting no further data about the auction, this method also
  // writes the AdsInterestGroup_AuctionLatency UKM entry.
  // IMPORTANT: Calling OnAuctionEnd invalidates this object. Almost all methods
  // called after the first all to OnAuctionEnd, including a second call to
  // OnAuctionEnd, will cause a crash.
  void OnAuctionEnd(AuctionResult result);

  // Records LoadInterestGroupPhaseLatency
  void OnLoadInterestGroupPhaseComplete();

  // Records several counts we have after loading all the InterestGroups. These
  // counts are all aggregated across all component auctions for a multi-seller
  // auction.
  void SetNumInterestGroups(int64_t num_interest_groups);
  void SetNumOwnersWithInterestGroups(int64_t num_interest_groups);
  void SetNumSellersWithBidders(int64_t num_sellers_with_bidders);

  // Reports an InterestGroup owner, used to determine the number of distinct
  // Buyers across all component auctions in a multi-seller auction.
  void ReportBuyer(url::Origin& owner);

  // Reports a Bidder WorkletKey, used to count the number of distinct
  // BidderWorklets in use by this auction, which might be more than the number
  // of owners with InterestGroups, because the same owner can only share a
  // worklet for InterestGroups that have the same bidding script URL and KV
  // server URL.BidderWorklets are potentially shared across different auctions
  // on the same page, so that it's possible for a BidderWorklet to be reused
  // from another auction. Since the NumBidderWorklets metric is looking for the
  // number of distinct BidderWorklets to compare against the number of distinct
  // Buyers, we count distinct BidderWorkletKeys instead of simply looking at
  // the number of BidderWorklets created in the context of a given auction.
  void ReportBidderWorkletKey(AuctionWorkletManager::WorkletKey& worklet_key);

  // Records InterestGroups that are loaded, but don't reach GenerateBid.
  // Some of these functions increment by a number of bids at once, while
  // others increment one at a time, depending on how they're used.
  void RecordBidsAbortedByBuyerCumulativeTimeout(int64_t num_bids);
  void RecordBidAbortedByBidderWorkletFatalError();
  void RecordBidFilteredDuringInterestGroupLoad();
  void RecordBidFilteredDuringReprioritization();
  void RecordBidsFilteredByPerBuyerLimits(int64_t num_bids);

  // Records the k-anonymity mode used for this auction.
  void SetKAnonymityBidMode(auction_worklet::mojom::KAnonymityBidMode bid_mode);

  // Records outcomes on the boundary between GenerateBid and ScoreAd.
  // Each of these is called once for each InterestGroup for which we called
  // GenerateBid.
  void RecordInterestGroupWithNoBids();
  void RecordInterestGroupWithOnlyNonKAnonBid();
  void RecordInterestGroupWithSameBidForKAnonAndNonKAnon();
  void RecordInterestGroupWithSeparateBidsForKAnonAndNonKAnon();

  // Records the latency of each component for a multi-seller auction.
  void RecordComponentAuctionLatency(base::TimeDelta latency);

  // Latency of the entire GenerateBid flow, including signals requests, for
  // a given BidState.
  void RecordBidForOneInterestGroupLatency(base::TimeDelta latency);
  // Latency of just the call to GenerateSingleBid.
  void RecordGenerateSingleBidLatency(base::TimeDelta latency);

 private:
  using UkmEntry = ukm::builders::AdsInterestGroup_AuctionLatency;
  using EntrySetFunction = UkmEntry& (UkmEntry::*)(int64_t value);

  // Helper class for aggregating latencies for events that occur many times
  // during the auction, for which we want to produce aggregate measurements
  // to record using separate metrics.
  class LatencyAggregator {
   public:
    LatencyAggregator() = default;
    LatencyAggregator(const LatencyAggregator&) = delete;
    LatencyAggregator& operator=(const LatencyAggregator&) = delete;

    void RecordLatency(base::TimeDelta latency);
    int32_t GetNumRecords();
    base::TimeDelta GetMeanLatency();
    base::TimeDelta GetMaxLatency();

   private:
    int32_t num_records_ = 0;
    base::TimeDelta sum_latency_;
    base::TimeDelta max_latency_;
  };

  // Helper function to set a pair of Mean and Max metrics only if the number of
  // records is non-zero.
  void MaybeSetMeanAndMaxLatency(LatencyAggregator& aggregator,
                                 EntrySetFunction set_mean_function,
                                 EntrySetFunction set_max_function);

  // The data structure we'll eventually record via the UkmRecorder.
  // We incrementally build this in all of the methods of this class.
  UkmEntry builder_;

  // Time at which AuctionRunner::StartAuction() is called; used as the
  // starting point for several of the latency metrics.
  base::TimeTicks auction_start_time_;

  // Set of distinct buyers across all component auctions. Only the size of
  // this set is recorded in UKM; the buyers are not.
  std::set<url::Origin> buyers_;

  // Set of distinct bidder worklet keys used to get BidderWorklets for this
  // auction across all component auctions. For memory efficiency, we record a
  // hash of the AuctionWorkletManager::WorkletKey instead of the key itself.
  std::set<size_t> bidder_worklet_keys_;

  // Counts of InterestGroups that were loaded, but didn't reach GenerateBid for
  // one of a number of reasons, some intentional for performance.
  int64_t num_bids_aborted_by_buyer_cumulative_timeout_ = 0;
  int64_t num_bids_aborted_by_bidder_worklet_fatal_error_ = 0;
  int64_t num_bids_filtered_during_interest_group_load_ = 0;
  int64_t num_bids_filtered_during_reprioritization_ = 0;
  int64_t num_bids_filtered_by_per_buyer_limits_ = 0;

  // Counts for outcomes on the boundary between GenerateBid and ScoreAd.
  // Incremented for each InterestGroup, and recorded OnAuctionEnd.
  int64_t num_interest_groups_with_no_bids_ = 0;
  int64_t num_interest_groups_with_only_non_k_anon_bid_ = 0;
  int64_t num_interest_groups_with_separate_bids_for_k_anon_and_non_k_anon_ = 0;
  int64_t num_interest_groups_with_same_bid_for_k_anon_and_non_k_anon_ = 0;

  // Various latency measurements.
  LatencyAggregator component_auction_latency_aggregator_;
  LatencyAggregator bid_for_one_interest_group_latency_aggregator_;
  LatencyAggregator generate_single_bid_latency_aggregator_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_METRICS_RECORDER_H_

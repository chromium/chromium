// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_METRICS_RECORDER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_METRICS_RECORDER_H_

#include <stdint.h>

#include <cstdint>
#include <optional>
#include <set>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "content/browser/interest_group/additional_bid_result.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/auction_result.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-shared.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

namespace content {

// AuctionMetricsRecorder instances need to outlive other objects that keep a
// reference to them, e.g. AuctionWorkletManager, so that these objects can
// effectively record metrics. AuctionMetricsRecorderManager is responsible for
// owning instances of AuctionMetricsRecorder beyond the lifetime of any of
// these referencing objects.
class CONTENT_EXPORT AuctionMetricsRecorderManager {
 public:
  explicit AuctionMetricsRecorderManager(ukm::SourceId ukm_source_id);
  ~AuctionMetricsRecorderManager();

  AuctionMetricsRecorderManager(const AuctionMetricsRecorderManager&) = delete;
  AuctionMetricsRecorderManager& operator=(
      const AuctionMetricsRecorderManager&) = delete;

  // Creates a new AuctionMetricsRecorder with the ukm_source_id provided in
  // the AuctionMetricsRecorderManager constructor. The
  // AuctionMetricsRecorderManager keeps ownership of this object and returns
  // a usable pointer so that other objects can use this to record metrics.
  AuctionMetricsRecorder* CreateAuctionMetricsRecorder();

 private:
  ukm::SourceId ukm_source_id_;
  std::vector<std::unique_ptr<AuctionMetricsRecorder>>
      owned_auction_metrics_recorders_;
};

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

  // This object's construction time is that recorded as the auction's start
  // time for measuring end-to-end latency, LoadInterestGroupPhase latency,
  // ConfigPromisesResolved latency, etc.
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

  // Records the times at which a buyer or seller worklet was requested.
  void OnWorkletRequested();

  // Records the times at which a buyer or seller worklet was ready.
  void OnWorkletReady();

  // Records how long it took for the config promises to be resolved since the
  // start of the auction. This is only called for auctions that have config
  // promises. For a multi-seller auction, this is called each time a component
  // auction has its config promises resolved.
  void OnConfigPromisesResolved();

  // Records several counts we have after loading all the InterestGroups. These
  // counts are all aggregated across all component auctions for a multi-seller
  // auction.
  void SetNumInterestGroups(int64_t num_interest_groups);
  void SetNumOwnersWithInterestGroups(int64_t num_owners_with_interest_groups);
  void SetNumOwnersWithoutInterestGroups(
      int64_t num_owners_without_interest_groups);
  void SetNumSellersWithBidders(int64_t num_sellers_with_bidders);

  // Records the number of negative interest groups associated with an auction.
  // In a multi-seller auction, this is incremented once for each component
  void RecordNegativeInterestGroups(int64_t num_negative_interest_groups);

  // Reports an InterestGroup owner, used to determine the number of distinct
  // Buyers across all component auctions in a multi-seller auction.
  void ReportBuyer(const url::Origin& owner);

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

  // Records the number of additional bids observed by the outcome of decoding.
  // This distinguishes between each of several error outcomes, as well as the
  // two non-error outcomes - additional bids that were negative targeted and
  // those that were sent for scoring.
  void RecordAdditionalBidResult(AdditionalBidResult result);

  // Records the time it took to decode an additional bid. This is separate from
  // the RecordAdditionalBidResult function above because this should only be
  // called for successfully decoded additional bids.
  void RecordAdditionalBidDecodeLatency(base::TimeDelta latency);

  // We also record cases where negative interest groups are not used,
  // potentially causing the additional bid to "fail open", i.e. to be scored
  // when it would not have been had the additional bid been considered.
  void RecordNegativeInterestGroupIgnoredDueToInvalidSignature();
  void RecordNegativeInterestGroupIgnoredDueToJoiningOriginMismatch();

  // Records the k-anonymity mode used for this auction.
  void SetKAnonymityBidMode(auction_worklet::mojom::KAnonymityBidMode bid_mode);

  // Records the total number of config promises for this auction. For a
  // multi-seller auction, this includes promises from both the top-level
  // auction and all component auctions within.
  void SetNumConfigPromises(int64_t num_config_promises);

  // Records outcomes on the boundary between GenerateBid and ScoreAd.
  // Each of these is called once for each InterestGroup for which we called
  // GenerateBid.
  void RecordInterestGroupWithNoBids();
  void RecordInterestGroupWithOnlyNonKAnonBid();
  void RecordInterestGroupWithSameBidForKAnonAndNonKAnon();
  void RecordInterestGroupWithSeparateBidsForKAnonAndNonKAnon();
  void RecordInterestGroupWithOtherMultiBid();

  // Records total number of bids returned from a generateBid() call, and the
  // number that's k-anonymous.
  void RecordNumberOfBidsFromGenerateBid(size_t k_anom_num, size_t num);

  // Records the latency of each component for a multi-seller auction.
  void RecordComponentAuctionLatency(base::TimeDelta latency);

  // Latency of the entire GenerateBid flow, including signals requests, for
  // a given BidState.
  void RecordBidForOneInterestGroupLatency(base::TimeDelta latency);
  // Latency of just the call to GenerateSingleBid.
  void RecordGenerateSingleBidLatency(base::TimeDelta latency);

  // Records latencies and critical path latencies of GenerateBid dependencies.
  void RecordGenerateBidDependencyLatencies(
      const auction_worklet::mojom::GenerateBidDependencyLatencies&
          generate_bid_dependency_latencies);

  // Records scoring delays due to unavailability of SellerWorklet or promises.
  void RecordTopLevelBidQueuedWaitingForConfigPromises(base::TimeDelta delay);
  void RecordTopLevelBidQueuedWaitingForSellerWorklet(base::TimeDelta delay);
  void RecordBidQueuedWaitingForConfigPromises(base::TimeDelta delay);
  void RecordBidQueuedWaitingForSellerWorklet(base::TimeDelta delay);

  // Latency of the entire ScoreAd flow, including signals requests, for
  // a given Bid.
  void RecordScoreAdFlowLatency(base::TimeDelta latency);
  // Latency of just the call to ScoreAd.
  void RecordScoreAdLatency(base::TimeDelta latency);

  // Records latencies and critical path latencies of ScoreAd dependencies.
  void RecordScoreAdDependencyLatencies(
      const auction_worklet::mojom::ScoreAdDependencyLatencies&
          score_ad_dependency_latencies);

 private:
  using UkmEntry = ukm::builders::AdsInterestGroup_AuctionLatency_V2;
  using EntrySetFunction = UkmEntry& (UkmEntry::*)(int64_t value);

  // Helper class for keeping track of the earliest recorded time among events
  // that may occur many times during the auction, and specifically phase start
  // times, used to better understand auction latency.
  class EarliestTimeRecorder {
   public:
    EarliestTimeRecorder() = default;
    EarliestTimeRecorder(const EarliestTimeRecorder&) = delete;
    EarliestTimeRecorder& operator=(const EarliestTimeRecorder&) = delete;

    // Records or overwrites the currently earliest time if this new time is
    // earlier than any previously recorded time.
    void MaybeRecordTime(base::TimeTicks time);

    std::optional<base::TimeTicks> get_earliest_time() {
      return earliest_time_;
    }

   private:
    std::optional<base::TimeTicks> earliest_time_;
  };

  // Helper class for keeping track of the latest recorded time among events
  // that may occur many times during the auction, and specifically phase end
  // times, used to better understand auction latency.
  class LatestTimeRecorder {
   public:
    LatestTimeRecorder() = default;
    LatestTimeRecorder(const LatestTimeRecorder&) = delete;
    LatestTimeRecorder& operator=(const LatestTimeRecorder&) = delete;

    // Records or overwrites the currently latest time if this new time is
    // later than any previously recorded time.
    void MaybeRecordTime(base::TimeTicks time);

    std::optional<base::TimeTicks> get_latest_time() { return latest_time_; }

   private:
    std::optional<base::TimeTicks> latest_time_;
  };

  // Helper function to set a pair of Mean and Max metrics only if the number of
  // records is non-zero.
  void MaybeSetMeanAndMaxLatency(LatencyAggregator& aggregator,
                                 EntrySetFunction set_mean_function,
                                 EntrySetFunction set_max_function);

  // Helper function to set a Num metric unconditionally, and set a Mean metric
  // only if the number of records is non-zero.
  void SetNumAndMaybeMeanLatency(LatencyAggregator& aggregator,
                                 EntrySetFunction set_num_function,
                                 EntrySetFunction set_mean_function);

  // Helper function to set a metric representing phase start time.
  void MaybeSetPhaseStartTime(EarliestTimeRecorder& earliest_start_time,
                              EntrySetFunction set_function);

  // Helper function to set a metric representing phase end time.
  void MaybeSetPhaseEndTime(LatestTimeRecorder& latest_end_time,
                            EntrySetFunction set_function);

  // Used internally to calculate GenerateBid Dependency critical path latency.
  struct GenerateBidDependencyCriticalPath {
    enum class Dependency {
      kCodeReady = 0,
      kConfigPromises = 1,
      kDirectFromSellerSignals = 2,
      kTrustedBiddingSignals = 3,
    };
    std::optional<Dependency> last_resolved_dependency;
    base::TimeDelta last_resolved_dependency_latency;
    base::TimeDelta penultimate_resolved_dependency_latency;
  };
  void MaybeRecordGenerateBidDependencyLatency(
      GenerateBidDependencyCriticalPath::Dependency dependency,
      std::optional<base::TimeDelta> latency,
      LatencyAggregator& aggregator,
      GenerateBidDependencyCriticalPath& critical_path);
  void RecordGenerateBidDependencyLatencyCriticalPath(
      GenerateBidDependencyCriticalPath& critical_path);
  void MaybeRecordGenerateBidPhasesStartAndEndTimes(
      const auction_worklet::mojom::GenerateBidDependencyLatencies&
          generate_bid_dependency_latencies);

  struct ScoreAdDependencyCriticalPath {
    enum class Dependency {
      kCodeReady = 0,
      kDirectFromSellerSignals = 2,
      kTrustedScoringSignals = 3,
    };
    std::optional<Dependency> last_resolved_dependency;
    base::TimeDelta last_resolved_dependency_latency;
    base::TimeDelta penultimate_resolved_dependency_latency;
  };
  void MaybeRecordScoreAdDependencyLatency(
      ScoreAdDependencyCriticalPath::Dependency dependency,
      std::optional<base::TimeDelta> latency,
      LatencyAggregator& aggregator,
      ScoreAdDependencyCriticalPath& critical_path);
  void RecordScoreAdDependencyLatencyCriticalPath(
      ScoreAdDependencyCriticalPath& critical_path);
  void MaybeRecordScoreAdPhasesStartAndEndTimes(
      const auction_worklet::mojom::ScoreAdDependencyLatencies&
          score_ad_dependency_latencies);

  // The data structure we'll eventually record via the UkmRecorder.
  // We incrementally build this in all of the methods of this class.
  UkmEntry builder_;

  // Time at which AuctionRunner::StartAuction() is called; used as the
  // starting point for several of the latency metrics.
  base::TimeTicks auction_start_time_;

  // Time at which the LoadInterestGroup phase completed and the
  // BiddingAndScoring phase began.
  std::optional<base::TimeTicks> bidding_and_scoring_phase_start_time_;

  // WorkletCreation phase metrics.
  EarliestTimeRecorder worklet_creation_phase_start_time_;
  LatestTimeRecorder worklet_creation_phase_end_time_;

  // Aggregate number of negative interest groups across all component auctions.
  // This only has a value if the auction (or any of the component auctions in
  // a multi-seller auction) provided a promise as the additionalBids field of
  // the auction config.
  std::optional<size_t> num_negative_interest_groups_;

  // Set of distinct buyers across all component auctions. Only the size of
  // this set is recorded in UKM; the buyers are not.
  std::set<url::Origin> buyers_;

  // Set of distinct bidder worklet keys used to get BidderWorklets for this
  // auction across all component auctions. For memory efficiency, we record a
  // hash of the AuctionWorkletManager::WorkletKey instead of the key itself.
  std::set<size_t> bidder_worklet_keys_;

  // Counts of InterestGroups that were loaded, but didn't reach GenerateBid
  // for one of a number of reasons, some intentional for performance.
  int64_t num_bids_aborted_by_buyer_cumulative_timeout_ = 0,
          num_bids_aborted_by_bidder_worklet_fatal_error_ = 0,
          num_bids_filtered_during_interest_group_load_ = 0,
          num_bids_filtered_during_reprioritization_ = 0,
          num_bids_filtered_by_per_buyer_limits_ = 0;

  // Counts and latencies of the time it took for config promises to be resolved
  // from the start of the auction. This is not the same as how long the auction
  // needed to "wait" for a config promise, since other work happened
  // simultaneously. That's better captured by the GenerateBid critical path
  // metrics and by metrics recorded using the aggregator below.
  LatencyAggregator config_promises_resolved_latency_aggregator_;

  // Counts and latencies of the time it took for config promises to be resolved
  // after the start of the BiddingAndScoring phase. This shows how long
  // additional bids would have had to wait for the config promises to be
  // resolved, since they couldn't be processed until the BiddingAndScoring
  // phase anyway.
  LatencyAggregator config_promises_resolved_critical_path_latency_aggregator_;

  // Counts of both error and non-error additional bid outcomes.
  base::flat_map<AdditionalBidResult, int64_t> additional_bid_result_counts_;

  // Counts and latencies of "successful" additional bid outcomes. This includes
  // both additional bids that are negative targeted and those that are sent for
  // scoring. We only measure latency on additional bids successfully decoded
  // because error latency varies too widely - almost immediate for a signed
  // additional bid with an invalid base64 encoding, but much longer (two JSON
  // parsings later) for an error found while decoding the additional bid.
  LatencyAggregator additional_bid_decode_latency_aggregator_;

  // Counts of negative interest groups ignored on additional bids. These are
  // the "failing open" scenarios described in the explainer.
  int64_t num_negative_interest_groups_ignored_due_to_invalid_signature_ = 0,
          num_negative_interest_groups_ignored_due_to_joining_origin_mismatch_ =
              0;

  // Counts for outcomes on the boundary between GenerateBid and ScoreAd.
  // Incremented for each InterestGroup, and recorded OnAuctionEnd.
  int64_t num_interest_groups_with_no_bids_ = 0,
          num_interest_groups_with_only_non_k_anon_bid_ = 0,
          num_interest_groups_with_separate_bids_for_k_anon_and_non_k_anon_ = 0,
          num_interest_groups_with_same_bid_for_k_anon_and_non_k_anon_ = 0,
          num_interest_groups_with_other_multi_bid_ = 0;

  // Number of bids generated from worklets invocation, total and k-anon
  // suitable ones only.
  size_t num_bids_generated_ = 0;
  size_t num_kanon_bids_generated_ = 0;

  // Various latency measurements.
  LatencyAggregator component_auction_latency_aggregator_;

  // GenerateBid latencies.
  LatencyAggregator bid_for_one_interest_group_latency_aggregator_,
      generate_single_bid_latency_aggregator_;

  // Aggregated latencies of GenerateBid dependencies.
  LatencyAggregator generate_bid_code_ready_latency_aggregator_,
      generate_bid_config_promises_latency_aggregator_,
      generate_bid_direct_from_seller_signals_latency_aggregator_,
      generate_bid_trusted_bidding_signals_latency_aggregator_;

  // Aggregated critical path latencies of GenerateBid dependencies.
  LatencyAggregator generate_bid_code_ready_critical_path_aggregator_,
      generate_bid_config_promises_critical_path_aggregator_,
      generate_bid_direct_from_seller_signals_critical_path_aggregator_,
      generate_bid_trusted_bidding_signals_critical_path_aggregator_;

  // GenerateBid phase metrics.
  EarliestTimeRecorder bid_signals_fetch_phase_start_time_;
  LatestTimeRecorder bid_signals_fetch_phase_end_time_;
  EarliestTimeRecorder bid_generation_phase_start_time_;
  LatestTimeRecorder bid_generation_phase_end_time_;

  // Aggregated critical path latencies of bids generated before the
  // corresponding SellerWorklet is ready.
  LatencyAggregator top_level_bid_queued_waiting_for_seller_worklet_aggregator_,
      top_level_bid_queued_waiting_for_config_promises_aggregator_,
      bid_queued_waiting_for_seller_worklet_aggregator_,
      bid_queued_waiting_for_config_promises_aggregator_;

  // ScoreAd latencies
  LatencyAggregator score_ad_flow_latency_aggregator_,
      score_ad_latency_aggregator_;

  // Aggregated latencies of ScoreAd dependencies.
  LatencyAggregator score_ad_code_ready_latency_aggregator_,
      score_ad_direct_from_seller_signals_latency_aggregator_,
      score_ad_trusted_scoring_signals_latency_aggregator_;

  // Aggregated critical path latencies of ScoreAd dependencies.
  LatencyAggregator score_ad_code_ready_critical_path_aggregator_,
      score_ad_direct_from_seller_signals_critical_path_aggregator_,
      score_ad_trusted_scoring_signals_critical_path_aggregator_;

  // ScoreAd phase metrics.
  EarliestTimeRecorder score_signals_fetch_phase_start_time_;
  LatestTimeRecorder score_signals_fetch_phase_end_time_;
  EarliestTimeRecorder scoring_phase_start_time_;
  LatestTimeRecorder scoring_phase_end_time_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_METRICS_RECORDER_H_

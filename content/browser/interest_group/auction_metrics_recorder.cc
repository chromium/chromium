// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_metrics_recorder.h"

#include <stdint.h>

#include <algorithm>

#include "base/check.h"
#include "base/functional/invoke.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_result.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-shared.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

namespace content {

using ukm::GetExponentialBucketMinForCounts1000;
using ukm::GetSemanticBucketMinForDurationTiming;

AuctionMetricsRecorder::AuctionMetricsRecorder(ukm::SourceId ukm_source_id)
    : builder_(ukm_source_id), auction_start_time_(base::TimeTicks::Now()) {}

AuctionMetricsRecorder::~AuctionMetricsRecorder() = default;

void AuctionMetricsRecorder::OnAuctionEnd(AuctionResult auction_result) {
  builder_.SetResult(static_cast<int64_t>(auction_result));
  base::TimeDelta e2e_latency = base::TimeTicks::Now() - auction_start_time_;
  builder_.SetEndToEndLatencyInMillis(
      GetSemanticBucketMinForDurationTiming(e2e_latency.InMilliseconds()));

  builder_.SetNumDistinctOwnersWithInterestGroups(
      GetExponentialBucketMinForCounts1000(buyers_.size()));

  builder_.SetNumBidderWorklets(
      GetExponentialBucketMinForCounts1000(bidder_worklet_keys_.size()));

  builder_.SetNumBidsAbortedByBuyerCumulativeTimeout(
      GetExponentialBucketMinForCounts1000(
          num_bids_aborted_by_buyer_cumulative_timeout_));
  builder_.SetNumBidsAbortedByBidderWorkletFatalError(
      GetExponentialBucketMinForCounts1000(
          num_bids_aborted_by_bidder_worklet_fatal_error_));
  builder_.SetNumBidsFilteredDuringInterestGroupLoad(
      GetExponentialBucketMinForCounts1000(
          num_bids_filtered_during_interest_group_load_));
  builder_.SetNumBidsFilteredDuringReprioritization(
      GetExponentialBucketMinForCounts1000(
          num_bids_filtered_during_reprioritization_));
  builder_.SetNumBidsFilteredByPerBuyerLimits(
      GetExponentialBucketMinForCounts1000(
          num_bids_filtered_by_per_buyer_limits_));

  builder_.SetNumInterestGroupsWithNoBids(
      GetExponentialBucketMinForCounts1000(num_interest_groups_with_no_bids_));
  builder_.SetNumInterestGroupsWithOnlyNonKAnonBid(
      GetExponentialBucketMinForCounts1000(
          num_interest_groups_with_only_non_k_anon_bid_));
  builder_.SetNumInterestGroupsWithSameBidForKAnonAndNonKAnon(
      GetExponentialBucketMinForCounts1000(
          num_interest_groups_with_same_bid_for_k_anon_and_non_k_anon_));
  builder_.SetNumInterestGroupsWithSeparateBidsForKAnonAndNonKAnon(
      GetExponentialBucketMinForCounts1000(
          num_interest_groups_with_separate_bids_for_k_anon_and_non_k_anon_));

  MaybeSetMeanAndMaxLatency(
      component_auction_latency_aggregator_,
      /*set_mean_function=*/&UkmEntry::SetMeanComponentAuctionLatencyInMillis,
      /*set_max_function=*/&UkmEntry::SetMaxComponentAuctionLatencyInMillis);

  MaybeSetMeanAndMaxLatency(
      bid_for_one_interest_group_latency_aggregator_,
      /*set_mean_function=*/
      &UkmEntry::SetMeanBidForOneInterestGroupLatencyInMillis,
      /*set_max_function=*/
      &UkmEntry::SetMaxBidForOneInterestGroupLatencyInMillis);
  MaybeSetMeanAndMaxLatency(
      generate_single_bid_latency_aggregator_,
      /*set_mean_function=*/&UkmEntry::SetMeanGenerateSingleBidLatencyInMillis,
      /*set_max_function=*/&UkmEntry::SetMaxGenerateSingleBidLatencyInMillis);

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  builder_.Record(ukm_recorder->Get());
}

void AuctionMetricsRecorder::OnLoadInterestGroupPhaseComplete() {
  base::TimeDelta load_interest_group_phase_latency =
      base::TimeTicks::Now() - auction_start_time_;
  builder_.SetLoadInterestGroupPhaseLatencyInMillis(
      GetSemanticBucketMinForDurationTiming(
          load_interest_group_phase_latency.InMilliseconds()));
}

void AuctionMetricsRecorder::SetNumInterestGroups(int64_t num_interest_groups) {
  builder_.SetNumInterestGroups(
      GetExponentialBucketMinForCounts1000(num_interest_groups));
}

void AuctionMetricsRecorder::SetNumOwnersWithInterestGroups(
    int64_t num_owners_with_interest_groups) {
  builder_.SetNumOwnersWithInterestGroups(
      GetExponentialBucketMinForCounts1000(num_owners_with_interest_groups));
}

void AuctionMetricsRecorder::SetNumSellersWithBidders(
    int64_t num_sellers_with_bidders) {
  builder_.SetNumSellersWithBidders(
      GetExponentialBucketMinForCounts1000(num_sellers_with_bidders));
}

void AuctionMetricsRecorder::ReportBuyer(url::Origin& owner) {
  buyers_.emplace(owner);
}

void AuctionMetricsRecorder::ReportBidderWorkletKey(
    AuctionWorkletManager::WorkletKey& worklet_key) {
  bidder_worklet_keys_.emplace(worklet_key.GetHash());
}

void AuctionMetricsRecorder::RecordBidsAbortedByBuyerCumulativeTimeout(
    int64_t num_bids) {
  num_bids_aborted_by_buyer_cumulative_timeout_ += num_bids;
}

void AuctionMetricsRecorder::RecordBidAbortedByBidderWorkletFatalError() {
  ++num_bids_aborted_by_bidder_worklet_fatal_error_;
}

void AuctionMetricsRecorder::RecordBidFilteredDuringInterestGroupLoad() {
  ++num_bids_filtered_during_interest_group_load_;
}

void AuctionMetricsRecorder::RecordBidFilteredDuringReprioritization() {
  ++num_bids_filtered_during_reprioritization_;
}

void AuctionMetricsRecorder::RecordBidsFilteredByPerBuyerLimits(
    int64_t num_bids) {
  num_bids_filtered_by_per_buyer_limits_ += num_bids;
}

void AuctionMetricsRecorder::SetKAnonymityBidMode(
    auction_worklet::mojom::KAnonymityBidMode bid_mode) {
  builder_.SetKAnonymityBidMode(static_cast<int64_t>(bid_mode));
}

void AuctionMetricsRecorder::RecordInterestGroupWithNoBids() {
  ++num_interest_groups_with_no_bids_;
}

void AuctionMetricsRecorder::RecordInterestGroupWithOnlyNonKAnonBid() {
  ++num_interest_groups_with_only_non_k_anon_bid_;
}

void AuctionMetricsRecorder::
    RecordInterestGroupWithSeparateBidsForKAnonAndNonKAnon() {
  ++num_interest_groups_with_separate_bids_for_k_anon_and_non_k_anon_;
}

void AuctionMetricsRecorder::
    RecordInterestGroupWithSameBidForKAnonAndNonKAnon() {
  ++num_interest_groups_with_same_bid_for_k_anon_and_non_k_anon_;
}

void AuctionMetricsRecorder::RecordComponentAuctionLatency(
    base::TimeDelta latency) {
  component_auction_latency_aggregator_.RecordLatency(latency);
}

void AuctionMetricsRecorder::RecordBidForOneInterestGroupLatency(
    base::TimeDelta latency) {
  bid_for_one_interest_group_latency_aggregator_.RecordLatency(latency);
}

void AuctionMetricsRecorder::RecordGenerateSingleBidLatency(
    base::TimeDelta latency) {
  generate_single_bid_latency_aggregator_.RecordLatency(latency);
}

void AuctionMetricsRecorder::LatencyAggregator::RecordLatency(
    base::TimeDelta latency) {
  // Negative latencies are meaningless; don't record them at all.
  if (latency.is_negative()) {
    return;
  }
  ++num_records_;
  sum_latency_ += latency;
  max_latency_ = std::max(latency, max_latency_);
}

int32_t AuctionMetricsRecorder::LatencyAggregator::GetNumRecords() {
  return num_records_;
}

base::TimeDelta AuctionMetricsRecorder::LatencyAggregator::GetMeanLatency() {
  if (num_records_ == 0) {
    return base::TimeDelta();
  }
  return sum_latency_ / num_records_;
}

base::TimeDelta AuctionMetricsRecorder::LatencyAggregator::GetMaxLatency() {
  return max_latency_;
}

void AuctionMetricsRecorder::MaybeSetMeanAndMaxLatency(
    AuctionMetricsRecorder::LatencyAggregator& aggregator,
    EntrySetFunction set_mean_function,
    EntrySetFunction set_max_function) {
  if (aggregator.GetNumRecords() > 0) {
    base::invoke(set_mean_function, builder_,
                 GetSemanticBucketMinForDurationTiming(
                     aggregator.GetMeanLatency().InMilliseconds()));
    base::invoke(set_max_function, builder_,
                 GetSemanticBucketMinForDurationTiming(
                     aggregator.GetMaxLatency().InMilliseconds()));
  }
}

}  // namespace content

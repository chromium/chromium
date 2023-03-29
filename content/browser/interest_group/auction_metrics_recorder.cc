// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_metrics_recorder.h"

#include <stdint.h>

#include "base/check.h"
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

}  // namespace content

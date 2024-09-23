// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_metrics_recorder.h"

#include <stdint.h>

#include <algorithm>
#include <functional>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/browser/interest_group/additional_bid_result.h"
#include "content/public/browser/auction_result.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-shared.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

namespace content {

using ukm::GetExponentialBucketMinForCounts1000;
using ukm::GetSemanticBucketMinForDurationTiming;

namespace {
int64_t GetBucketMinForPhaseTimeMetric(base::TimeDelta time_delta) {
  return ukm::GetLinearBucketMin(time_delta.InMilliseconds(),
                                 /*bucket_size=*/10);
}
}  // namespace

AuctionMetricsRecorderManager::AuctionMetricsRecorderManager(
    ukm::SourceId ukm_source_id)
    : ukm_source_id_(ukm_source_id) {}

AuctionMetricsRecorderManager::~AuctionMetricsRecorderManager() = default;

AuctionMetricsRecorder*
AuctionMetricsRecorderManager::CreateAuctionMetricsRecorder() {
  auto auction_metrics_recorder =
      std::make_unique<AuctionMetricsRecorder>(ukm_source_id_);
  AuctionMetricsRecorder* auction_metrics_recorder_ptr =
      auction_metrics_recorder.get();
  owned_auction_metrics_recorders_.push_back(
      std::move(auction_metrics_recorder));
  return auction_metrics_recorder_ptr;
}

AuctionMetricsRecorder::AuctionMetricsRecorder(ukm::SourceId ukm_source_id)
    : builder_(ukm_source_id), auction_start_time_(base::TimeTicks::Now()) {}

AuctionMetricsRecorder::~AuctionMetricsRecorder() = default;

void AuctionMetricsRecorder::OnAuctionEnd(AuctionResult auction_result) {
  builder_.SetResult(static_cast<int64_t>(auction_result));
  base::TimeDelta e2e_latency = base::TimeTicks::Now() - auction_start_time_;
  builder_.SetEndToEndLatencyInMillis(
      GetSemanticBucketMinForDurationTiming(e2e_latency.InMilliseconds()));

  MaybeSetPhaseStartTime(worklet_creation_phase_start_time_,
                         &UkmEntry::SetWorkletCreationPhaseStartTimeInMillis);
  MaybeSetPhaseEndTime(worklet_creation_phase_end_time_,
                       &UkmEntry::SetWorkletCreationPhaseEndTimeInMillis);

  if (num_negative_interest_groups_) {
    builder_.SetNumNegativeInterestGroups(
        GetExponentialBucketMinForCounts1000(*num_negative_interest_groups_));
    base::UmaHistogramCustomCounts(
        "Ads.InterestGroup.Auction.NumNegativeInterestGroups",
        *num_negative_interest_groups_,
        /*min=*/1, /*exclusive_max=*/20000, /*buckets=*/50);
  }

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

  builder_.SetNumAdditionalBidsSentForScoring(
      GetExponentialBucketMinForCounts1000(
          additional_bid_result_counts_[AdditionalBidResult::kSentForScoring]));
  builder_.SetNumAdditionalBidsNegativeTargeted(
      GetExponentialBucketMinForCounts1000(
          additional_bid_result_counts_
              [AdditionalBidResult::kNegativeTargeted]));
  builder_.SetNumAdditionalBidsRejectedDueToInvalidBase64(
      GetExponentialBucketMinForCounts1000(
          additional_bid_result_counts_
              [AdditionalBidResult::kRejectedDueToInvalidBase64]));
  builder_.SetNumAdditionalBidsRejectedDueToSignedBidJsonParseError(
      GetExponentialBucketMinForCounts1000(
          additional_bid_result_counts_
              [AdditionalBidResult::kRejectedDueToSignedBidJsonParseError]));
  builder_.SetNumAdditionalBidsRejectedDueToSignedBidDecodeError(
      GetExponentialBucketMinForCounts1000(
          additional_bid_result_counts_
              [AdditionalBidResult::kRejectedDueToSignedBidDecodeError]));
  builder_.SetNumAdditionalBidsRejectedDueToJsonParseError(
      GetExponentialBucketMinForCounts1000(
          additional_bid_result_counts_
              [AdditionalBidResult::kRejectedDueToJsonParseError]));
  builder_.SetNumAdditionalBidsRejectedDueToDecodeError(
      GetExponentialBucketMinForCounts1000(
          additional_bid_result_counts_
              [AdditionalBidResult::kRejectedDueToDecodeError]));
  builder_.SetNumAdditionalBidsRejectedDueToBuyerNotAllowed(
      GetExponentialBucketMinForCounts1000(
          additional_bid_result_counts_
              [AdditionalBidResult::kRejectedDueToBuyerNotAllowed]));
  builder_.SetNumAdditionalBidsRejectedDueToCurrencyMismatch(
      GetExponentialBucketMinForCounts1000(
          additional_bid_result_counts_
              [AdditionalBidResult::kRejectedDueToCurrencyMismatch]));

  MaybeSetMeanAndMaxLatency(
      additional_bid_decode_latency_aggregator_,
      /*set_mean_function=*/
      &UkmEntry::SetMeanAdditionalBidDecodeLatencyInMillis,
      /*set_max_function=*/
      &UkmEntry::SetMaxAdditionalBidDecodeLatencyInMillis);

  builder_.SetNumNegativeInterestGroupsIgnoredDueToInvalidSignature(
      GetExponentialBucketMinForCounts1000(
          num_negative_interest_groups_ignored_due_to_invalid_signature_));
  builder_.SetNumNegativeInterestGroupsIgnoredDueToJoiningOriginMismatch(
      GetExponentialBucketMinForCounts1000(
          num_negative_interest_groups_ignored_due_to_joining_origin_mismatch_));

  builder_.SetNumAuctionsWithConfigPromises(
      GetExponentialBucketMinForCounts1000(
          config_promises_resolved_latency_aggregator_.GetNumRecords()));
  MaybeSetMeanAndMaxLatency(
      config_promises_resolved_latency_aggregator_,
      /*set_mean_function=*/
      &UkmEntry::SetMeanConfigPromisesResolvedLatencyInMillis,
      /*set_max_function=*/
      &UkmEntry::SetMaxConfigPromisesResolvedLatencyInMillis);
  MaybeSetMeanAndMaxLatency(
      config_promises_resolved_critical_path_latency_aggregator_,
      /*set_mean_function=*/
      &UkmEntry::SetMeanConfigPromisesResolvedCriticalPathLatencyInMillis,
      /*set_max_function=*/
      &UkmEntry::SetMaxConfigPromisesResolvedCriticalPathLatencyInMillis);

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
  builder_.SetNumInterestGroupsWithOtherMultiBid(
      GetExponentialBucketMinForCounts1000(
          num_interest_groups_with_other_multi_bid_));

  if (num_bids_generated_) {
    base::UmaHistogramPercentage(
        "Ads.InterestGroup.Auction.PercentBidsKAnon",
        100 * num_kanon_bids_generated_ / num_bids_generated_);
  }

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

  MaybeSetMeanAndMaxLatency(
      generate_bid_code_ready_latency_aggregator_,
      /*set_mean_function=*/
      &UkmEntry::SetMeanGenerateBidCodeReadyLatencyInMillis,
      /*set_max_function=*/
      &UkmEntry::SetMaxGenerateBidCodeReadyLatencyInMillis);
  MaybeSetMeanAndMaxLatency(
      generate_bid_config_promises_latency_aggregator_,
      /*set_mean_function=*/
      &UkmEntry::SetMeanGenerateBidConfigPromisesLatencyInMillis,
      /*set_max_function=*/
      &UkmEntry::SetMaxGenerateBidConfigPromisesLatencyInMillis);
  MaybeSetMeanAndMaxLatency(
      generate_bid_direct_from_seller_signals_latency_aggregator_,
      /*set_mean_function=*/
      &UkmEntry::SetMeanGenerateBidDirectFromSellerSignalsLatencyInMillis,
      /*set_max_function=*/
      &UkmEntry::SetMaxGenerateBidDirectFromSellerSignalsLatencyInMillis);
  MaybeSetMeanAndMaxLatency(
      generate_bid_trusted_bidding_signals_latency_aggregator_,
      /*set_mean_function=*/
      &UkmEntry::SetMeanGenerateBidTrustedBiddingSignalsLatencyInMillis,
      /*set_max_function=*/
      &UkmEntry::SetMaxGenerateBidTrustedBiddingSignalsLatencyInMillis);

  SetNumAndMaybeMeanLatency(
      generate_bid_code_ready_critical_path_aggregator_,
      /*set_num_function=*/
      &UkmEntry::SetNumGenerateBidCodeReadyOnCriticalPath,
      /*set_mean_function=*/
      &UkmEntry::SetMeanGenerateBidCodeReadyCriticalPathLatencyInMillis);
  SetNumAndMaybeMeanLatency(
      generate_bid_config_promises_critical_path_aggregator_,
      /*set_num_function=*/
      &UkmEntry::SetNumGenerateBidConfigPromisesOnCriticalPath,
      /*set_mean_function=*/
      &UkmEntry::SetMeanGenerateBidConfigPromisesCriticalPathLatencyInMillis);
  SetNumAndMaybeMeanLatency(
      generate_bid_direct_from_seller_signals_critical_path_aggregator_,
      /*set_num_function=*/
      &UkmEntry::SetNumGenerateBidDirectFromSellerSignalsOnCriticalPath,
      /*set_mean_function=*/
      &UkmEntry::
          SetMeanGenerateBidDirectFromSellerSignalsCriticalPathLatencyInMillis);
  SetNumAndMaybeMeanLatency(
      generate_bid_trusted_bidding_signals_critical_path_aggregator_,
      /*set_num_function=*/
      &UkmEntry::SetNumGenerateBidTrustedBiddingSignalsOnCriticalPath,
      /*set_mean_function=*/
      &UkmEntry::
          SetMeanGenerateBidTrustedBiddingSignalsCriticalPathLatencyInMillis);

  MaybeSetPhaseStartTime(bid_signals_fetch_phase_start_time_,
                         &UkmEntry::SetBidSignalsFetchPhaseStartTimeInMillis);
  MaybeSetPhaseEndTime(bid_signals_fetch_phase_end_time_,
                       &UkmEntry::SetBidSignalsFetchPhaseEndTimeInMillis);
  MaybeSetPhaseStartTime(bid_generation_phase_start_time_,
                         &UkmEntry::SetBidGenerationPhaseStartTimeInMillis);
  MaybeSetPhaseEndTime(bid_generation_phase_end_time_,
                       &UkmEntry::SetBidGenerationPhaseEndTimeInMillis);

  SetNumAndMaybeMeanLatency(
      top_level_bid_queued_waiting_for_config_promises_aggregator_,
      /*set_num_function=*/
      &UkmEntry::SetNumTopLevelBidsQueuedWaitingForConfigPromises,
      /*set_mean_function=*/
      &UkmEntry::SetMeanTimeTopLevelBidsQueuedWaitingForConfigPromisesInMillis);
  SetNumAndMaybeMeanLatency(
      top_level_bid_queued_waiting_for_seller_worklet_aggregator_,
      /*set_num_function=*/
      &UkmEntry::SetNumTopLevelBidsQueuedWaitingForSellerWorklet,
      /*set_mean_function=*/
      &UkmEntry::SetMeanTimeTopLevelBidsQueuedWaitingForSellerWorkletInMillis);

  SetNumAndMaybeMeanLatency(
      bid_queued_waiting_for_config_promises_aggregator_,
      /*set_num_function=*/&UkmEntry::SetNumBidsQueuedWaitingForConfigPromises,
      /*set_mean_function=*/
      &UkmEntry::SetMeanTimeBidsQueuedWaitingForConfigPromisesInMillis);
  SetNumAndMaybeMeanLatency(
      bid_queued_waiting_for_seller_worklet_aggregator_,
      /*set_num_function=*/&UkmEntry::SetNumBidsQueuedWaitingForSellerWorklet,
      /*set_mean_function=*/
      &UkmEntry::SetMeanTimeBidsQueuedWaitingForSellerWorkletInMillis);

  MaybeSetMeanAndMaxLatency(score_ad_latency_aggregator_,
                            /*set_mean_function=*/
                            &UkmEntry::SetMeanScoreAdLatencyInMillis,
                            /*set_max_function=*/
                            &UkmEntry::SetMaxScoreAdLatencyInMillis);
  MaybeSetMeanAndMaxLatency(
      score_ad_flow_latency_aggregator_,
      /*set_mean_function=*/&UkmEntry::SetMeanScoreAdFlowLatencyInMillis,
      /*set_max_function=*/&UkmEntry::SetMaxScoreAdFlowLatencyInMillis);

  MaybeSetMeanAndMaxLatency(score_ad_code_ready_latency_aggregator_,
                            /*set_mean_function=*/
                            &UkmEntry::SetMeanScoreAdCodeReadyLatencyInMillis,
                            /*set_max_function=*/
                            &UkmEntry::SetMaxScoreAdCodeReadyLatencyInMillis);
  MaybeSetMeanAndMaxLatency(
      score_ad_direct_from_seller_signals_latency_aggregator_,
      /*set_mean_function=*/
      &UkmEntry::SetMeanScoreAdDirectFromSellerSignalsLatencyInMillis,
      /*set_max_function=*/
      &UkmEntry::SetMaxScoreAdDirectFromSellerSignalsLatencyInMillis);
  MaybeSetMeanAndMaxLatency(
      score_ad_trusted_scoring_signals_latency_aggregator_,
      /*set_mean_function=*/
      &UkmEntry::SetMeanScoreAdTrustedScoringSignalsLatencyInMillis,
      /*set_max_function=*/
      &UkmEntry::SetMaxScoreAdTrustedScoringSignalsLatencyInMillis);

  SetNumAndMaybeMeanLatency(
      score_ad_code_ready_critical_path_aggregator_,
      /*set_num_function=*/
      &UkmEntry::SetNumScoreAdCodeReadyOnCriticalPath,
      /*set_mean_function=*/
      &UkmEntry::SetMeanScoreAdCodeReadyCriticalPathLatencyInMillis);
  SetNumAndMaybeMeanLatency(
      score_ad_direct_from_seller_signals_critical_path_aggregator_,
      /*set_num_function=*/
      &UkmEntry::SetNumScoreAdDirectFromSellerSignalsOnCriticalPath,
      /*set_mean_function=*/
      &UkmEntry::
          SetMeanScoreAdDirectFromSellerSignalsCriticalPathLatencyInMillis);
  SetNumAndMaybeMeanLatency(
      score_ad_trusted_scoring_signals_critical_path_aggregator_,
      /*set_num_function=*/
      &UkmEntry::SetNumScoreAdTrustedScoringSignalsOnCriticalPath,
      /*set_mean_function=*/
      &UkmEntry::
          SetMeanScoreAdTrustedScoringSignalsCriticalPathLatencyInMillis);

  MaybeSetPhaseStartTime(score_signals_fetch_phase_start_time_,
                         &UkmEntry::SetScoreSignalsFetchPhaseStartTimeInMillis);
  MaybeSetPhaseEndTime(score_signals_fetch_phase_end_time_,
                       &UkmEntry::SetScoreSignalsFetchPhaseEndTimeInMillis);
  MaybeSetPhaseStartTime(scoring_phase_start_time_,
                         &UkmEntry::SetScoringPhaseStartTimeInMillis);
  MaybeSetPhaseEndTime(scoring_phase_end_time_,
                       &UkmEntry::SetScoringPhaseEndTimeInMillis);

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  builder_.Record(ukm_recorder->Get());
}

void AuctionMetricsRecorder::OnLoadInterestGroupPhaseComplete() {
  DCHECK(!bidding_and_scoring_phase_start_time_.has_value());
  base::TimeTicks now = base::TimeTicks::Now();
  bidding_and_scoring_phase_start_time_ = now;

  base::TimeDelta load_interest_group_phase_latency = now - auction_start_time_;
  builder_.SetLoadInterestGroupPhaseLatencyInMillis(
      GetSemanticBucketMinForDurationTiming(
          load_interest_group_phase_latency.InMilliseconds()));
  builder_.SetLoadInterestGroupPhaseEndTimeInMillis(
      GetBucketMinForPhaseTimeMetric(load_interest_group_phase_latency));
}

void AuctionMetricsRecorder::OnWorkletRequested() {
  worklet_creation_phase_start_time_.MaybeRecordTime(base::TimeTicks::Now());
}

void AuctionMetricsRecorder::OnWorkletReady() {
  worklet_creation_phase_end_time_.MaybeRecordTime(base::TimeTicks::Now());
}

void AuctionMetricsRecorder::OnConfigPromisesResolved() {
  base::TimeTicks now = base::TimeTicks::Now();

  base::TimeDelta latency = now - auction_start_time_;
  config_promises_resolved_latency_aggregator_.RecordLatency(latency);
  base::UmaHistogramTimes("Ads.InterestGroup.Auction.ConfigPromises.Latency",
                          latency);

  base::TimeDelta critical_path_latency =
      bidding_and_scoring_phase_start_time_.has_value()
          ? now - bidding_and_scoring_phase_start_time_.value()
          : base::Microseconds(0);
  config_promises_resolved_critical_path_latency_aggregator_.RecordLatency(
      critical_path_latency);
  base::UmaHistogramTimes(
      "Ads.InterestGroup.Auction.ConfigPromises.CriticalPathLatency",
      critical_path_latency);
}

void AuctionMetricsRecorder::SetNumInterestGroups(int64_t num_interest_groups) {
  builder_.SetNumInterestGroups(
      GetExponentialBucketMinForCounts1000(num_interest_groups));
  base::UmaHistogramCustomCounts(
      "Ads.InterestGroup.Auction.NumInterestGroups", num_interest_groups,
      /*min=*/1, /*exclusive_max=*/2000, /*buckets=*/50);
}

void AuctionMetricsRecorder::SetNumOwnersWithInterestGroups(
    int64_t num_owners_with_interest_groups) {
  builder_.SetNumOwnersWithInterestGroups(
      GetExponentialBucketMinForCounts1000(num_owners_with_interest_groups));
  base::UmaHistogramCounts100(
      "Ads.InterestGroup.Auction.NumOwnersWithInterestGroups",
      num_owners_with_interest_groups);
}

void AuctionMetricsRecorder::SetNumOwnersWithoutInterestGroups(
    int64_t num_owners_without_interest_groups) {
  builder_.SetNumOwnersWithoutInterestGroups(
      GetExponentialBucketMinForCounts1000(num_owners_without_interest_groups));
  base::UmaHistogramCounts100(
      "Ads.InterestGroup.Auction.NumOwnersWithoutInterestGroups",
      num_owners_without_interest_groups);
}

void AuctionMetricsRecorder::SetNumSellersWithBidders(
    int64_t num_sellers_with_bidders) {
  builder_.SetNumSellersWithBidders(
      GetExponentialBucketMinForCounts1000(num_sellers_with_bidders));
  base::UmaHistogramCounts100("Ads.InterestGroup.Auction.NumSellersWithBidders",
                              num_sellers_with_bidders);
}

void AuctionMetricsRecorder::RecordNegativeInterestGroups(
    int64_t num_negative_interest_groups) {
  if (!num_negative_interest_groups_) {
    num_negative_interest_groups_ = 0;
  }
  *num_negative_interest_groups_ += num_negative_interest_groups;
}

void AuctionMetricsRecorder::ReportBuyer(const url::Origin& owner) {
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

void AuctionMetricsRecorder::RecordAdditionalBidResult(
    AdditionalBidResult result) {
  ++additional_bid_result_counts_[result];
  base::UmaHistogramEnumeration(
      "Ads.InterestGroup.Auction.AdditionalBids.Result", result);
}

void AuctionMetricsRecorder::RecordAdditionalBidDecodeLatency(
    base::TimeDelta latency) {
  base::UmaHistogramTimes(
      "Ads.InterestGroup.Auction.AdditionalBids.DecodeLatency", latency);
  additional_bid_decode_latency_aggregator_.RecordLatency(latency);
}

void AuctionMetricsRecorder::
    RecordNegativeInterestGroupIgnoredDueToInvalidSignature() {
  ++num_negative_interest_groups_ignored_due_to_invalid_signature_;
}

void AuctionMetricsRecorder::
    RecordNegativeInterestGroupIgnoredDueToJoiningOriginMismatch() {
  ++num_negative_interest_groups_ignored_due_to_joining_origin_mismatch_;
}

void AuctionMetricsRecorder::SetKAnonymityBidMode(
    auction_worklet::mojom::KAnonymityBidMode bid_mode) {
  builder_.SetKAnonymityBidMode(static_cast<int64_t>(bid_mode));
}

void AuctionMetricsRecorder::SetNumConfigPromises(int64_t num_config_promises) {
  builder_.SetNumConfigPromises(
      GetExponentialBucketMinForCounts1000(num_config_promises));
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

void AuctionMetricsRecorder::RecordInterestGroupWithOtherMultiBid() {
  ++num_interest_groups_with_other_multi_bid_;
}

void AuctionMetricsRecorder::RecordNumberOfBidsFromGenerateBid(
    size_t k_anom_num,
    size_t total_num) {
  num_kanon_bids_generated_ += k_anom_num;
  num_bids_generated_ += total_num;
  base::UmaHistogramCounts100(
      "Ads.InterestGroup.Auction.NumBidsGeneratedAtOnce", total_num);
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

void AuctionMetricsRecorder::RecordGenerateBidDependencyLatencies(
    const auction_worklet::mojom::GenerateBidDependencyLatencies&
        generate_bid_dependency_latencies) {
  GenerateBidDependencyCriticalPath critical_path;

  MaybeRecordGenerateBidDependencyLatency(
      GenerateBidDependencyCriticalPath::Dependency::kCodeReady,
      generate_bid_dependency_latencies.code_ready_latency,
      generate_bid_code_ready_latency_aggregator_, critical_path);

  MaybeRecordGenerateBidDependencyLatency(
      GenerateBidDependencyCriticalPath::Dependency::kConfigPromises,
      generate_bid_dependency_latencies.config_promises_latency,
      generate_bid_config_promises_latency_aggregator_, critical_path);

  MaybeRecordGenerateBidDependencyLatency(
      GenerateBidDependencyCriticalPath::Dependency::kDirectFromSellerSignals,
      generate_bid_dependency_latencies.direct_from_seller_signals_latency,
      generate_bid_direct_from_seller_signals_latency_aggregator_,
      critical_path);

  MaybeRecordGenerateBidDependencyLatency(
      GenerateBidDependencyCriticalPath::Dependency::kTrustedBiddingSignals,
      generate_bid_dependency_latencies.trusted_bidding_signals_latency,
      generate_bid_trusted_bidding_signals_latency_aggregator_, critical_path);

  RecordGenerateBidDependencyLatencyCriticalPath(critical_path);

  MaybeRecordGenerateBidPhasesStartAndEndTimes(
      generate_bid_dependency_latencies);
}

void AuctionMetricsRecorder::RecordTopLevelBidQueuedWaitingForConfigPromises(
    base::TimeDelta delay) {
  top_level_bid_queued_waiting_for_config_promises_aggregator_.RecordLatency(
      delay);
}

void AuctionMetricsRecorder::RecordTopLevelBidQueuedWaitingForSellerWorklet(
    base::TimeDelta delay) {
  top_level_bid_queued_waiting_for_seller_worklet_aggregator_.RecordLatency(
      delay);
}

void AuctionMetricsRecorder::RecordBidQueuedWaitingForConfigPromises(
    base::TimeDelta delay) {
  bid_queued_waiting_for_config_promises_aggregator_.RecordLatency(delay);
}

void AuctionMetricsRecorder::RecordBidQueuedWaitingForSellerWorklet(
    base::TimeDelta delay) {
  bid_queued_waiting_for_seller_worklet_aggregator_.RecordLatency(delay);
}

void AuctionMetricsRecorder::RecordScoreAdFlowLatency(base::TimeDelta latency) {
  score_ad_flow_latency_aggregator_.RecordLatency(latency);
}

void AuctionMetricsRecorder::RecordScoreAdLatency(base::TimeDelta latency) {
  score_ad_latency_aggregator_.RecordLatency(latency);
}

void AuctionMetricsRecorder::RecordScoreAdDependencyLatencies(
    const auction_worklet::mojom::ScoreAdDependencyLatencies&
        score_ad_dependency_latencies) {
  ScoreAdDependencyCriticalPath critical_path;

  MaybeRecordScoreAdDependencyLatency(
      ScoreAdDependencyCriticalPath::Dependency::kCodeReady,
      score_ad_dependency_latencies.code_ready_latency,
      score_ad_code_ready_latency_aggregator_, critical_path);

  MaybeRecordScoreAdDependencyLatency(
      ScoreAdDependencyCriticalPath::Dependency::kDirectFromSellerSignals,
      score_ad_dependency_latencies.direct_from_seller_signals_latency,
      score_ad_direct_from_seller_signals_latency_aggregator_, critical_path);

  MaybeRecordScoreAdDependencyLatency(
      ScoreAdDependencyCriticalPath::Dependency::kTrustedScoringSignals,
      score_ad_dependency_latencies.trusted_scoring_signals_latency,
      score_ad_trusted_scoring_signals_latency_aggregator_, critical_path);

  RecordScoreAdDependencyLatencyCriticalPath(critical_path);

  MaybeRecordScoreAdPhasesStartAndEndTimes(score_ad_dependency_latencies);
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

void AuctionMetricsRecorder::EarliestTimeRecorder::MaybeRecordTime(
    base::TimeTicks time) {
  if (!earliest_time_.has_value() || time < *earliest_time_) {
    earliest_time_ = time;
  }
}

void AuctionMetricsRecorder::LatestTimeRecorder::MaybeRecordTime(
    base::TimeTicks time) {
  if (!latest_time_.has_value() || time > *latest_time_) {
    latest_time_ = time;
  }
}

void AuctionMetricsRecorder::MaybeSetMeanAndMaxLatency(
    AuctionMetricsRecorder::LatencyAggregator& aggregator,
    EntrySetFunction set_mean_function,
    EntrySetFunction set_max_function) {
  if (aggregator.GetNumRecords() > 0) {
    std::invoke(set_mean_function, builder_,
                GetSemanticBucketMinForDurationTiming(
                    aggregator.GetMeanLatency().InMilliseconds()));
    std::invoke(set_max_function, builder_,
                GetSemanticBucketMinForDurationTiming(
                    aggregator.GetMaxLatency().InMilliseconds()));
  }
}

void AuctionMetricsRecorder::SetNumAndMaybeMeanLatency(
    AuctionMetricsRecorder::LatencyAggregator& aggregator,
    EntrySetFunction set_num_function,
    EntrySetFunction set_mean_function) {
  std::invoke(set_num_function, builder_,
              GetExponentialBucketMinForCounts1000(aggregator.GetNumRecords()));
  if (aggregator.GetNumRecords() > 0) {
    std::invoke(set_mean_function, builder_,
                GetSemanticBucketMinForDurationTiming(
                    aggregator.GetMeanLatency().InMilliseconds()));
  }
}

void AuctionMetricsRecorder::MaybeSetPhaseStartTime(
    EarliestTimeRecorder& recorder,
    EntrySetFunction set_function) {
  std::optional<base::TimeTicks> earliest_start_time =
      recorder.get_earliest_time();
  if (earliest_start_time.has_value()) {
    std::invoke(set_function, builder_,
                GetBucketMinForPhaseTimeMetric(*earliest_start_time -
                                               auction_start_time_));
  }
}

void AuctionMetricsRecorder::MaybeSetPhaseEndTime(
    LatestTimeRecorder& recorder,
    EntrySetFunction set_function) {
  std::optional<base::TimeTicks> latest_end_time = recorder.get_latest_time();
  if (latest_end_time.has_value()) {
    std::invoke(
        set_function, builder_,
        GetBucketMinForPhaseTimeMetric(*latest_end_time - auction_start_time_));
  }
}

void AuctionMetricsRecorder::MaybeRecordGenerateBidDependencyLatency(
    GenerateBidDependencyCriticalPath::Dependency dependency,
    std::optional<base::TimeDelta> latency,
    LatencyAggregator& aggregator,
    GenerateBidDependencyCriticalPath& critical_path) {
  if (latency) {
    aggregator.RecordLatency(*latency);
    if (*latency >= critical_path.last_resolved_dependency_latency) {
      critical_path.last_resolved_dependency = dependency;
      critical_path.penultimate_resolved_dependency_latency =
          critical_path.last_resolved_dependency_latency;
      critical_path.last_resolved_dependency_latency = *latency;
    } else if (*latency >
               critical_path.penultimate_resolved_dependency_latency) {
      critical_path.penultimate_resolved_dependency_latency = *latency;
    }
  }
}

void AuctionMetricsRecorder::RecordGenerateBidDependencyLatencyCriticalPath(
    GenerateBidDependencyCriticalPath& critical_path) {
  if (!critical_path.last_resolved_dependency.has_value()) {
    return;
  }
  base::TimeDelta critical_path_latency =
      critical_path.last_resolved_dependency_latency -
      critical_path.penultimate_resolved_dependency_latency;
  using Dependency = GenerateBidDependencyCriticalPath::Dependency;
  switch (*critical_path.last_resolved_dependency) {
    case Dependency::kCodeReady:
      generate_bid_code_ready_critical_path_aggregator_.RecordLatency(
          critical_path_latency);
      break;
    case Dependency::kConfigPromises:
      generate_bid_config_promises_critical_path_aggregator_.RecordLatency(
          critical_path_latency);
      break;
    case Dependency::kDirectFromSellerSignals:
      generate_bid_direct_from_seller_signals_critical_path_aggregator_
          .RecordLatency(critical_path_latency);
      break;
    case Dependency::kTrustedBiddingSignals:
      generate_bid_trusted_bidding_signals_critical_path_aggregator_
          .RecordLatency(critical_path_latency);
      break;
  }
}

void AuctionMetricsRecorder::MaybeRecordGenerateBidPhasesStartAndEndTimes(
    const auction_worklet::mojom::GenerateBidDependencyLatencies&
        generate_bid_dependency_latencies) {
  bid_signals_fetch_phase_start_time_.MaybeRecordTime(
      generate_bid_dependency_latencies.deps_wait_start_time);
  bid_signals_fetch_phase_end_time_.MaybeRecordTime(
      generate_bid_dependency_latencies.generate_bid_start_time);
  bid_generation_phase_start_time_.MaybeRecordTime(
      generate_bid_dependency_latencies.generate_bid_start_time);
  bid_generation_phase_end_time_.MaybeRecordTime(
      generate_bid_dependency_latencies.generate_bid_finish_time);
}

void AuctionMetricsRecorder::MaybeRecordScoreAdDependencyLatency(
    ScoreAdDependencyCriticalPath::Dependency dependency,
    std::optional<base::TimeDelta> latency,
    LatencyAggregator& aggregator,
    ScoreAdDependencyCriticalPath& critical_path) {
  if (latency) {
    aggregator.RecordLatency(*latency);
    if (*latency >= critical_path.last_resolved_dependency_latency) {
      critical_path.last_resolved_dependency = dependency;
      critical_path.penultimate_resolved_dependency_latency =
          critical_path.last_resolved_dependency_latency;
      critical_path.last_resolved_dependency_latency = *latency;
    } else if (*latency >
               critical_path.penultimate_resolved_dependency_latency) {
      critical_path.penultimate_resolved_dependency_latency = *latency;
    }
  }
}

void AuctionMetricsRecorder::RecordScoreAdDependencyLatencyCriticalPath(
    ScoreAdDependencyCriticalPath& critical_path) {
  if (!critical_path.last_resolved_dependency.has_value()) {
    return;
  }
  base::TimeDelta critical_path_latency =
      critical_path.last_resolved_dependency_latency -
      critical_path.penultimate_resolved_dependency_latency;
  using Dependency = ScoreAdDependencyCriticalPath::Dependency;
  switch (*critical_path.last_resolved_dependency) {
    case Dependency::kCodeReady:
      score_ad_code_ready_critical_path_aggregator_.RecordLatency(
          critical_path_latency);
      break;
    case Dependency::kDirectFromSellerSignals:
      score_ad_direct_from_seller_signals_critical_path_aggregator_
          .RecordLatency(critical_path_latency);
      break;
    case Dependency::kTrustedScoringSignals:
      score_ad_trusted_scoring_signals_critical_path_aggregator_.RecordLatency(
          critical_path_latency);
      break;
  }
}

void AuctionMetricsRecorder::MaybeRecordScoreAdPhasesStartAndEndTimes(
    const auction_worklet::mojom::ScoreAdDependencyLatencies&
        score_ad_dependency_latencies) {
  score_signals_fetch_phase_start_time_.MaybeRecordTime(
      score_ad_dependency_latencies.deps_wait_start_time);
  score_signals_fetch_phase_end_time_.MaybeRecordTime(
      score_ad_dependency_latencies.score_ad_start_time);
  scoring_phase_start_time_.MaybeRecordTime(
      score_ad_dependency_latencies.score_ad_start_time);
  scoring_phase_end_time_.MaybeRecordTime(
      score_ad_dependency_latencies.score_ad_finish_time);
}

}  // namespace content

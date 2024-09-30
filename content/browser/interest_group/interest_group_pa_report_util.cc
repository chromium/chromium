// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_pa_report_util.h"

#include <stdint.h>

#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/cpp/private_aggregation_reporting.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Returns the actual value of `base_value` with corresponding post auction
// signal such as `winning_bid`. Returns std::nullopt if corresponding signal
// is not available.
std::optional<double> GetBaseValue(
    auction_worklet::mojom::BaseValue base_value,
    double winning_bid,
    double highest_scoring_other_bid,
    const std::optional<auction_worklet::mojom::RejectReason> reject_reason,
    const PrivateAggregationParticipantData& participant_data,
    const PrivateAggregationTimings& timings) {
  base::UmaHistogramEnumeration("Ads.InterestGroup.Auction.PABaseValueUsed",
                                base_value);
  // The mojom API declaration should ensure base_value is one of these cases.
  switch (base_value) {
    case auction_worklet::mojom::BaseValue::kWinningBid:
      return winning_bid;
    case auction_worklet::mojom::BaseValue::kHighestScoringOtherBid:
      return highest_scoring_other_bid;
    case auction_worklet::mojom::BaseValue::kScriptRunTime:
      return timings.script_run_time.InMillisecondsF();
    case auction_worklet::mojom::BaseValue::kSignalsFetchTime:
      return timings.signals_fetch_time.InMillisecondsF();
    case auction_worklet::mojom::BaseValue::kBidRejectReason:
      // reportWin() and reportResult() have no reject reason, so their private
      // aggregation requests with "bid-reject-reason" base value are not sent.
      // If scoreAd() doesn't return a reject reason, it's reported as
      // kNotAvailable, instead of not being reported.
      if (reject_reason.has_value()) {
        return static_cast<int>(reject_reason.value());
      }
      return std::nullopt;
    case auction_worklet::mojom::BaseValue::kParticipatingInterestGroupCount:
      return participant_data.participating_interest_group_count;
    case auction_worklet::mojom::BaseValue::kAverageCodeFetchTime:
      return participant_data.average_code_fetch_time.InMillisecondsF();
    case auction_worklet::mojom::BaseValue::kPercentScriptsTimeout:
      return participant_data.percent_scripts_timeout;
    case auction_worklet::mojom::BaseValue::
        kPercentInterestGroupsCumulativeTimeout:
      return participant_data.percent_igs_cumulative_timeout;
    case auction_worklet::mojom::BaseValue::kCumulativeBuyerTime:
      return participant_data.cumulative_buyer_time.InMillisecondsF();
    case auction_worklet::mojom::BaseValue::kRegularInterestGroupsUsed:
      return participant_data.regular_igs;
    case auction_worklet::mojom::BaseValue::
        kPercentRegularInterestGroupQuotaUsed:
      return participant_data.percent_regular_igs_quota_used;
    case auction_worklet::mojom::BaseValue::kNegativeInterestGroupsUsed:
      return participant_data.negative_igs;
    case auction_worklet::mojom::BaseValue::
        kPercentNegativeInterestGroupQuotaUsed:
      return participant_data.percent_negative_igs_quota_used;
    case auction_worklet::mojom::BaseValue::kInterestGroupStorageUsed:
      return participant_data.igs_storage_used;
    case auction_worklet::mojom::BaseValue::
        kPercentInterestGroupStorageQuotaUsed:
      return participant_data.percent_igs_storage_quota_used;
  }
  NOTREACHED();
}

// Returns contribution's bucket calculated from `base`, and `bucket_obj`'s
// scale and offset. Returns std::nullopt if `base` is std::nullopt, or base
// or scale is NaN/infinity. Applies saturation arithmetic (in which all
// operations are limited to a fixed range) to uint128 bucket (also applied to
// intermediate results when they are too large to fit into a uint128). The
// fixed range is 0 to absl::Uint128Max().
std::optional<absl::uint128> CalculateBucket(
    const auction_worklet::mojom::SignalBucketPtr& bucket_obj,
    std::optional<double> base) {
  if (!base.has_value()) {
    return std::nullopt;
  }

  // The multiplication of base value with scale is performed using double
  // precision floating point numbers, as specified in the explainer.
  // https://github.com/WICG/turtledove/blob/main/FLEDGE_extended_PA_reporting.md
  double scaled_base_value = base.value() * bucket_obj->scale;

  // Returns std::nullopt if scaled_base_value is NaN.
  // TODO(crbug.com/40254312): Throw a bad message if scale is NaN or infinity.
  if (std::isnan(scaled_base_value)) {
    return std::nullopt;
  }

  bool scaled_base_value_is_negative = std::signbit(scaled_base_value);
  if (std::isinf(scaled_base_value) ||
      std::abs(scaled_base_value) >= std::ldexp(1.0L, 128)) {
    // Clamps to max value of uint128 when it overflows due to too high a
    // result, or clamps to 0 if it is negative i.e., base.value() * scale is
    // not finite or cannot fit into uint128. Also returns it since adding the
    // offset to an overflow number doesn't make sense.
    return scaled_base_value_is_negative ? 0 : absl::Uint128Max();
  }

  // May truncate the floating point result when converting to an integer.
  absl::uint128 abs_scaled_base_value =
      absl::uint128(std::abs(scaled_base_value));

  auction_worklet::mojom::BucketOffsetPtr offset =
      std::move(bucket_obj->offset);
  if (!offset) {
    return scaled_base_value_is_negative ? 0 : abs_scaled_base_value;
  }

  if (offset->is_negative) {
    return (scaled_base_value_is_negative ||
            abs_scaled_base_value < offset->value)
               ? 0
               : abs_scaled_base_value - offset->value;
  }

  if (scaled_base_value_is_negative) {
    // Clamps if offset - abs_scale_base_value < 0.
    return offset->value < abs_scaled_base_value
               ? 0
               : offset->value - abs_scaled_base_value;
  }

  // Clamps if the sum of abs_scale_base_value and offset overflows due to too
  // big.
  return abs_scaled_base_value > absl::Uint128Max() - offset->value
             ? absl::Uint128Max()
             : offset->value + abs_scaled_base_value;
}

// Returns contribution's value calculated from `base`, and `value_obj`'s scale
// and offset. Returns 0 if the calculated value is negative. Returns
// std::nullopt if `base` is std::nullopt, or base or scale is NaN/infinity.
std::optional<int32_t> CalculateValue(
    const auction_worklet::mojom::SignalValuePtr& value_obj,
    std::optional<double> base) {
  if (!base.has_value()) {
    return std::nullopt;
  }

  double scaled_base_value = base.value() * value_obj->scale;
  // Returns std::nullopt if the product of base and scale is NaN.
  // TODO(crbug.com/40254312): Throw a bad message if scale is NaN or infinity.
  if (std::isnan(scaled_base_value)) {
    return std::nullopt;
  }

  // Note: truncates the floating point result, without losing precision since
  // doubles can store all 32-bit integers exactly. Saturating as needed. Mojom
  // should guarantee offset being int32.
  base::ClampedNumeric<int32_t> value = scaled_base_value + value_obj->offset;

  // Returns 0 if value is negative, since Private Aggregation API and the
  // aggregation service does not support negative value in contribution.
  if (value < 0) {
    return 0;
  }
  return value;
}

// Calculates given for-event `contribution`'s bucket and value with given post
// auction signals such as `winning_bid`, and returns a histogram contribution
// with calculated bucket and value. A negative value will be clamped to 0.
// Returns nullptr if `contribution`'s bucket cannot be calculated to a valid
// uint128 number, or `contribution`'s value cannot be calculated to a valid
// integer.
blink::mojom::AggregatableReportHistogramContributionPtr
CalculateContributionBucketAndValue(
    auction_worklet::mojom::AggregatableReportForEventContributionPtr
        contribution,
    double winning_bid,
    double highest_scoring_other_bid,
    const std::optional<auction_worklet::mojom::RejectReason> reject_reason,
    const PrivateAggregationParticipantData& participant_data,
    const PrivateAggregationTimings& timings) {
  absl::uint128 bucket;
  int value;

  if (contribution->bucket->is_id_bucket()) {
    bucket = contribution->bucket->get_id_bucket();
  } else {
    auction_worklet::mojom::SignalBucketPtr& bucket_obj =
        contribution->bucket->get_signal_bucket();
    std::optional<absl::uint128> bucket_opt = CalculateBucket(
        bucket_obj, GetBaseValue(bucket_obj->base_value, winning_bid,
                                 highest_scoring_other_bid, reject_reason,
                                 participant_data, timings));
    if (!bucket_opt.has_value()) {
      return nullptr;
    }
    bucket = bucket_opt.value();
  }

  if (contribution->value->is_int_value()) {
    value = contribution->value->get_int_value();
    if (value < 0) {
      // Clamps value to 0 if it's negative. The worklet code should prevent
      // this, but the worklet process may be compromised. Since it has no
      // effect on the result of the auction, we just clamp it to 0 instead of
      // terminate the auction.
      // TODO(crbug.com/40254406): Report a bad mojom message when int value is
      // negative.
      value = 0;
    }
  } else {
    const auction_worklet::mojom::SignalValuePtr& value_obj =
        contribution->value->get_signal_value();
    std::optional<int> value_opt = CalculateValue(
        value_obj, GetBaseValue(value_obj->base_value, winning_bid,
                                highest_scoring_other_bid, reject_reason,
                                participant_data, timings));
    if (!value_opt.has_value()) {
      return nullptr;
    }
    value = value_opt.value();
  }

  std::optional<uint64_t> filtering_id;
  if (base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiFilteringIds)) {
    filtering_id = contribution->filtering_id;
  }

  return blink::mojom::AggregatableReportHistogramContribution::New(
      bucket, value, filtering_id);
}

}  // namespace

PrivateAggregationParticipantData::PrivateAggregationParticipantData() =
    default;

PrivateAggregationParticipantData::PrivateAggregationParticipantData(
    const PrivateAggregationParticipantData& other) = default;
PrivateAggregationParticipantData& PrivateAggregationParticipantData::operator=(
    const PrivateAggregationParticipantData& other) = default;

PrivateAggregationParticipantData::PrivateAggregationParticipantData(
    PrivateAggregationParticipantData&& other) = default;
PrivateAggregationParticipantData& PrivateAggregationParticipantData::operator=(
    PrivateAggregationParticipantData&& other) = default;

PrivateAggregationKey::PrivateAggregationKey(
    url::Origin reporting_origin,
    std::optional<url::Origin> aggregation_coordinator_origin)
    : reporting_origin(std::move(reporting_origin)),
      aggregation_coordinator_origin(
          std::move(aggregation_coordinator_origin)) {}

PrivateAggregationKey::PrivateAggregationKey(
    const PrivateAggregationKey& other) = default;

PrivateAggregationKey& PrivateAggregationKey::operator=(
    const PrivateAggregationKey& other) = default;

PrivateAggregationKey::PrivateAggregationKey(PrivateAggregationKey&& other) =
    default;

PrivateAggregationKey& PrivateAggregationKey::operator=(
    PrivateAggregationKey&& other) = default;

PrivateAggregationKey::~PrivateAggregationKey() = default;

PrivateAggregationPhaseKey::PrivateAggregationPhaseKey(
    url::Origin reporting_origin,
    PrivateAggregationPhase phase,
    std::optional<url::Origin> aggregation_coordinator_origin)
    : reporting_origin(reporting_origin),
      phase(phase),
      aggregation_coordinator_origin(aggregation_coordinator_origin) {}

PrivateAggregationPhaseKey::PrivateAggregationPhaseKey(
    const PrivateAggregationPhaseKey&) = default;

PrivateAggregationPhaseKey& PrivateAggregationPhaseKey::operator=(
    const PrivateAggregationPhaseKey&) = default;

PrivateAggregationPhaseKey::PrivateAggregationPhaseKey(
    PrivateAggregationPhaseKey&&) = default;

PrivateAggregationPhaseKey& PrivateAggregationPhaseKey::operator=(
    PrivateAggregationPhaseKey&&) = default;

PrivateAggregationPhaseKey::~PrivateAggregationPhaseKey() = default;

PrivateAggregationRequestWithEventType::PrivateAggregationRequestWithEventType(
    auction_worklet::mojom::PrivateAggregationRequestPtr request,
    std::optional<std::string> event_type)
    : request(std::move(request)), event_type(event_type) {}

PrivateAggregationRequestWithEventType::PrivateAggregationRequestWithEventType(
    PrivateAggregationRequestWithEventType&&) = default;

bool PrivateAggregationRequestWithEventType::operator==(
    const PrivateAggregationRequestWithEventType& rhs) const {
  return request == rhs.request && event_type == rhs.event_type;
}

PrivateAggregationRequestWithEventType::
    ~PrivateAggregationRequestWithEventType() = default;

std::optional<PrivateAggregationRequestWithEventType>
FillInPrivateAggregationRequest(
    auction_worklet::mojom::PrivateAggregationRequestPtr request,
    double winning_bid,
    double highest_scoring_other_bid,
    const std::optional<auction_worklet::mojom::RejectReason> reject_reason,
    const PrivateAggregationParticipantData& participant_data,
    const PrivateAggregationTimings& timings,
    bool is_winner) {
  CHECK(request);
  if (request->contribution->is_histogram_contribution()) {
    // TODO(crbug.com/40254406): Report a bad mojom message when contribution's
    // value is negative. The worklet code should prevent that, but the worklet
    // process may be compromised.
    PrivateAggregationRequestWithEventType request_with_event_type(
        std::move(request), /*event_type=*/std::nullopt);
    return request_with_event_type;
  }

  auction_worklet::mojom::AggregatableReportContributionPtr contribution =
      std::move(request->contribution);

  // The mojom API declaration should ensure `contribution` being a
  // for-event contribution if not a histogram contribution.
  CHECK(contribution->is_for_event_contribution());
  const auction_worklet::mojom::EventTypePtr& event_type =
      contribution->get_for_event_contribution()->event_type;
  std::optional<std::string> non_reserved_event_type = std::nullopt;
  std::optional<auction_worklet::mojom::ReservedEventType> reserved_event_type =
      std::nullopt;
  if (event_type->is_non_reserved()) {
    non_reserved_event_type = event_type->get_non_reserved();
  } else {
    reserved_event_type = event_type->get_reserved();
  }

  if (is_winner) {
    // Don't run loss events for a winner.
    if (reserved_event_type.has_value() &&
        *reserved_event_type ==
            auction_worklet::mojom::ReservedEventType::kReservedLoss) {
      return std::nullopt;
    }
  } else {
    // Private aggregation requests of non reserved event types are not kept for
    // losing bidders.
    if (non_reserved_event_type.has_value()) {
      return std::nullopt;
    }

    // Don't run win events for a loser.
    if (reserved_event_type.has_value() &&
        *reserved_event_type ==
            auction_worklet::mojom::ReservedEventType::kReservedWin) {
      return std::nullopt;
    }
  }

  blink::mojom::AggregatableReportHistogramContributionPtr
      calculated_contribution = CalculateContributionBucketAndValue(
          std::move(contribution->get_for_event_contribution()), winning_bid,
          highest_scoring_other_bid, reject_reason, participant_data, timings);
  if (!calculated_contribution) {
    return std::nullopt;
  }

  PrivateAggregationRequestWithEventType request_with_event_type(
      auction_worklet::mojom::PrivateAggregationRequest::New(
          auction_worklet::mojom::AggregatableReportContribution::
              NewHistogramContribution(std::move(calculated_contribution)),
          request->aggregation_mode, std::move(request->debug_mode_details)),
      non_reserved_event_type);
  return request_with_event_type;
}

bool IsPrivateAggregationRequestReservedOnce(
    const auction_worklet::mojom::PrivateAggregationRequest& request) {
  if (request.contribution->is_histogram_contribution()) {
    return false;
  }
  return request.contribution->get_for_event_contribution()
             ->event_type->is_reserved() &&
         request.contribution->get_for_event_contribution()
                 ->event_type->get_reserved() ==
             auction_worklet::mojom::ReservedEventType::kReservedOnce;
}

void SplitContributionsIntoBatchesThenSendToHost(
    std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr> requests,
    PrivateAggregationManager& pa_manager,
    const url::Origin& reporting_origin,
    std::optional<url::Origin> aggregation_coordinator_origin,
    const url::Origin& main_frame_origin) {
  CHECK_EQ(reporting_origin.scheme(), url::kHttpsScheme);

  // Split the vector of requests into those with matching debug mode details.
  std::map<
      blink::mojom::DebugModeDetailsPtr,
      std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>>
      contributions_map;

  bool is_debug_mode_allowed = pa_manager.IsDebugModeAllowed(
      /*top_frame_origin=*/main_frame_origin, reporting_origin);

  for (auction_worklet::mojom::PrivateAggregationRequestPtr& request :
       requests) {
    // All for-event contributions have already been converted to histogram
    // contributions by filling in post auction signals using
    // `FillInPrivateAggregationRequest()` before reaching here.
    CHECK(request->contribution->is_histogram_contribution());
    CHECK(request->debug_mode_details);

    // TODO(alexmt): Split by this too when it can be non-default.
    CHECK_EQ(request->aggregation_mode,
             blink::mojom::AggregationServiceMode::kDefault);

    // If debug mode will be ignored by the Private Aggregation layer, we
    // override the value here to allow the contributions to be batched
    // together.
    if (!is_debug_mode_allowed) {
      request->debug_mode_details = blink::mojom::DebugModeDetails::New();
    }

    contributions_map[std::move(request->debug_mode_details)].push_back(
        std::move(request->contribution->get_histogram_contribution()));
  }

  if (aggregation_coordinator_origin &&
      !aggregation_service::IsAggregationCoordinatorOriginAllowed(
          aggregation_coordinator_origin.value())) {
    // Ignore contributions that use an invalid coordinator.
    return;
  }

  for (auto& [debug_mode_details, contributions] : contributions_map) {
    mojo::Remote<blink::mojom::PrivateAggregationHost> remote_host;

    bool bound = pa_manager.BindNewReceiver(
        /*worklet_origin=*/reporting_origin,
        /*top_frame_origin=*/main_frame_origin,
        PrivateAggregationCallerApi::kProtectedAudience,
        /*context_id=*/std::nullopt,
        /*timeout=*/std::nullopt, aggregation_coordinator_origin,
        PrivateAggregationHost::kDefaultFilteringIdMaxBytes,
        remote_host.BindNewPipeAndPassReceiver());

    // The worklet origin should be potentially trustworthy (and no context ID
    // is set) and we checked the coordinator origin, so this should always
    // succeed.
    CHECK(bound);

    if (debug_mode_details->is_enabled) {
      remote_host->EnableDebugMode(std::move(debug_mode_details->debug_key));
    }
    remote_host->ContributeToHistogram(std::move(contributions));
    remote_host.reset();
  }
}

bool HasValidFilteringId(
    const auction_worklet::mojom::PrivateAggregationRequestPtr& request) {
  std::optional<uint64_t> filtering_id;
  if (request->contribution->is_histogram_contribution()) {
    filtering_id =
        request->contribution->get_histogram_contribution()->filtering_id;
  } else {
    CHECK(request->contribution->is_for_event_contribution());
    filtering_id =
        request->contribution->get_for_event_contribution()->filtering_id;
  }
  return IsValidFilteringId(filtering_id);
}

bool IsValidFilteringId(std::optional<uint64_t> filtering_id) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiFilteringIds)) {
    return filtering_id == std::nullopt;
  }

  return filtering_id.value_or(0) <= 255;
}

std::optional<std::string> ValidatePrivateAggregationRequests(
    const std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>&
        pa_requests) {
  bool additional_extensions_allowed = base::FeatureList::IsEnabled(
      blink::features::
          kPrivateAggregationApiProtectedAudienceAdditionalExtensions);
  for (const auto& request : pa_requests) {
    // The mojom API declaration should ensure none of these are null.
    CHECK(!request.is_null());

    if (!HasValidFilteringId(request)) {
      return "Private Aggregation filtering ID invalid";
    }

    if (!auction_worklet::
            IsValidPrivateAggregationRequestForAdditionalExtensions(
                *request, additional_extensions_allowed)) {
      return "Private Aggregation request using disabled features";
    }
  }
  return std::nullopt;
}

}  // namespace content

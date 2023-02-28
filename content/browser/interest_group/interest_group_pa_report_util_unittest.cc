// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_pa_report_util.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"

namespace content {
namespace {

const PrivateAggregationRequestWithEventType
    kExpectedRequestWithReservedEventType(
        auction_worklet::mojom::PrivateAggregationRequest::New(
            auction_worklet::mojom::AggregatableReportContribution::
                NewHistogramContribution(
                    blink::mojom::AggregatableReportHistogramContribution::New(
                        /*bucket=*/123,
                        /*value=*/45)),
            blink::mojom::AggregationServiceMode::kDefault,
            blink::mojom::DebugModeDetails::New()),
        /*event_type=*/absl::nullopt);

auction_worklet::mojom::SignalBucketPtr CreateSignalBucket(
    double scale,
    absl::uint128 offset_value,
    bool is_negative,
    auction_worklet::mojom::BaseValue base_value =
        auction_worklet::mojom::BaseValue::kWinningBid) {
  return auction_worklet::mojom::SignalBucket::New(
      base_value, scale,
      /*offset=*/
      auction_worklet::mojom::BucketOffset::New(offset_value, is_negative));
}

auction_worklet::mojom::SignalValuePtr CreateSignalValue(
    double scale,
    int32_t offset,
    auction_worklet::mojom::BaseValue base_value =
        auction_worklet::mojom::BaseValue::kWinningBid) {
  return auction_worklet::mojom::SignalValue::New(base_value, scale, offset);
}

// Creates a PrivateAggregationRequest with histogram contribution using
// uint128 `bucket` and int `value`.
auction_worklet::mojom::PrivateAggregationRequestPtr CreateHistogramRequest(
    absl::uint128 bucket,
    int32_t value) {
  return auction_worklet::mojom::PrivateAggregationRequest::New(
      auction_worklet::mojom::AggregatableReportContribution::
          NewHistogramContribution(
              blink::mojom::AggregatableReportHistogramContribution::New(
                  bucket, value)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
}

// Creates a PrivateAggregationRequest with ForEvent contribution using
// uint128 `bucket` and int `value`.
auction_worklet::mojom::PrivateAggregationRequestPtr CreateForEventRequest(
    absl::uint128 bucket,
    int32_t value,
    const std::string& event_type) {
  auto contribution =
      auction_worklet::mojom::AggregatableReportForEventContribution::New(
          auction_worklet::mojom::ForEventSignalBucket::NewIdBucket(bucket),
          auction_worklet::mojom::ForEventSignalValue::NewIntValue(value),
          event_type);

  return auction_worklet::mojom::PrivateAggregationRequest::New(
      auction_worklet::mojom::AggregatableReportContribution::
          NewForEventContribution(std::move(contribution)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
}

// Creates a PrivateAggregationRequest with ForEvent contribution using
// `bucket` object and int `value`.
auction_worklet::mojom::PrivateAggregationRequestPtr
CreateForEventRequestWithBucketObject(
    auction_worklet::mojom::SignalBucketPtr bucket,
    int32_t value,
    const std::string& event_type) {
  auto contribution =
      auction_worklet::mojom::AggregatableReportForEventContribution::New(
          auction_worklet::mojom::ForEventSignalBucket::NewSignalBucket(
              std::move(bucket)),
          auction_worklet::mojom::ForEventSignalValue::NewIntValue(value),
          event_type);

  return auction_worklet::mojom::PrivateAggregationRequest::New(
      auction_worklet::mojom::AggregatableReportContribution::
          NewForEventContribution(std::move(contribution)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
}

// Creates a PrivateAggregationRequest with ForEvent contribution using
// uint128 `bucket` and `value` object.
auction_worklet::mojom::PrivateAggregationRequestPtr
CreateForEventRequestWithValueObject(
    absl::uint128 bucket,
    auction_worklet::mojom::SignalValuePtr value,
    const std::string& event_type) {
  auto contribution =
      auction_worklet::mojom::AggregatableReportForEventContribution::New(
          auction_worklet::mojom::ForEventSignalBucket::NewIdBucket(bucket),
          auction_worklet::mojom::ForEventSignalValue::NewSignalValue(
              std::move(value)),
          event_type);

  return auction_worklet::mojom::PrivateAggregationRequest::New(
      auction_worklet::mojom::AggregatableReportContribution::
          NewForEventContribution(std::move(contribution)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());
}

PrivateAggregationRequestWithEventType
CreatePrivateAggregationRequestWithEventType(
    auction_worklet::mojom::PrivateAggregationRequestPtr request,
    const absl::optional<std::string>& event_type = absl::nullopt) {
  PrivateAggregationRequestWithEventType result(std::move(request), event_type);
  return result;
}

}  // namespace

class InterestGroupPaReportUtilTest : public testing::Test {
 public:
  InterestGroupPaReportUtilTest() = default;

  ~InterestGroupPaReportUtilTest() override = default;
};

// Expects FillInPrivateAggregationRequest() returns the same request if its
// input `request` is a HistogramContribution, regardless of is_winner being
// true or false.
TEST_F(InterestGroupPaReportUtilTest, HistogramContribution) {
  auction_worklet::mojom::PrivateAggregationRequest request(
      auction_worklet::mojom::AggregatableReportContribution::
          NewHistogramContribution(
              blink::mojom::AggregatableReportHistogramContribution::New(
                  /*bucket=*/123,
                  /*value=*/45)),
      blink::mojom::AggregationServiceMode::kDefault,
      blink::mojom::DebugModeDetails::New());

  EXPECT_EQ(kExpectedRequestWithReservedEventType,
            FillInPrivateAggregationRequest(
                request.Clone(),
                /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/true));

  EXPECT_EQ(kExpectedRequestWithReservedEventType,
            FillInPrivateAggregationRequest(
                request.Clone(),
                /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/false));
}

// FillInPrivateAggregationRequest() sets returned request's
// aggregation_mode and debug_mode_details correctly.
TEST_F(InterestGroupPaReportUtilTest, AggregationModeAndDebugMode) {
  auction_worklet::mojom::PrivateAggregationRequest request(
      auction_worklet::mojom::AggregatableReportContribution::
          NewHistogramContribution(
              blink::mojom::AggregatableReportHistogramContribution::New(
                  /*bucket=*/123,
                  /*value=*/45)),
      blink::mojom::AggregationServiceMode::kExperimentalPoplar,
      blink::mojom::DebugModeDetails::New(
          /*is_enabled=*/true,
          /*debug_key=*/blink::mojom::DebugKey::New(1234u)));

  PrivateAggregationRequestWithEventType request_with_event_type(
      request.Clone(), /*event_type=*/absl::nullopt);
  EXPECT_EQ(std::move(request_with_event_type),
            FillInPrivateAggregationRequest(
                request.Clone(),
                /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/true));
}

TEST_F(InterestGroupPaReportUtilTest, ForEventContributionReservedEventType) {
  EXPECT_EQ(kExpectedRequestWithReservedEventType,
            FillInPrivateAggregationRequest(
                CreateForEventRequest(/*bucket=*/123, /*value=*/45,
                                      /*event_type=*/kReservedAlways),
                /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/true));
  EXPECT_EQ(kExpectedRequestWithReservedEventType,
            FillInPrivateAggregationRequest(
                CreateForEventRequest(/*bucket=*/123, /*value=*/45,
                                      /*event_type=*/kReservedAlways),
                /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/false));

  EXPECT_EQ(kExpectedRequestWithReservedEventType,
            FillInPrivateAggregationRequest(
                CreateForEventRequest(/*bucket=*/123, /*value=*/45,
                                      /*event_type=*/kReservedWin),
                /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/true));
  EXPECT_FALSE(FillInPrivateAggregationRequest(
      CreateForEventRequest(/*bucket=*/123, /*value=*/45,
                            /*event_type=*/kReservedWin),
      /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
      /*reject_reason=*/absl::nullopt, /*is_winner=*/false));

  EXPECT_EQ(kExpectedRequestWithReservedEventType,
            FillInPrivateAggregationRequest(
                CreateForEventRequest(/*bucket=*/123, /*value=*/45,
                                      /*event_type=*/kReservedLoss),
                /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/false));
  EXPECT_FALSE(FillInPrivateAggregationRequest(
      CreateForEventRequest(/*bucket=*/123, /*value=*/45,
                            /*event_type=*/kReservedLoss),
      /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
      /*reject_reason=*/absl::nullopt, /*is_winner=*/true));

  EXPECT_FALSE(FillInPrivateAggregationRequest(
      CreateForEventRequest(/*bucket=*/123, /*value=*/45,
                            /*event_type=*/"reserved.not-supported"),
      /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
      /*reject_reason=*/absl::nullopt, /*is_winner=*/true));
}

TEST_F(InterestGroupPaReportUtilTest,
       ForEventContributionNonReservedEventType) {
  EXPECT_EQ(CreatePrivateAggregationRequestWithEventType(
                CreateHistogramRequest(/*bucket=*/123, /*value=*/45),
                /*event_type=*/"click"),
            FillInPrivateAggregationRequest(
                CreateForEventRequest(/*bucket=*/123, /*value=*/45,
                                      /*event_type=*/"click"),
                /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/true));

  EXPECT_EQ(CreatePrivateAggregationRequestWithEventType(
                CreateHistogramRequest(/*bucket=*/123, /*value=*/45),
                /*event_type=*/"arbitrary.non.reserved"),
            FillInPrivateAggregationRequest(
                CreateForEventRequest(/*bucket=*/123, /*value=*/45,
                                      /*event_type=*/"arbitrary.non.reserved"),
                /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/true));

  // The prefix is "reserved", not "reserved.", so still a valid non-reserved
  // event type.
  EXPECT_EQ(CreatePrivateAggregationRequestWithEventType(
                CreateHistogramRequest(/*bucket=*/123, /*value=*/45),
                /*event_type=*/"reserved-no-dot"),
            FillInPrivateAggregationRequest(
                CreateForEventRequest(/*bucket=*/123, /*value=*/45,
                                      /*event_type=*/"reserved-no-dot"),
                /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/true));

  // Requests of non-reserved event types are not kept for losing bidders.
  EXPECT_FALSE(FillInPrivateAggregationRequest(
      CreateForEventRequest(/*bucket=*/123, /*value=*/45,
                            /*event_type=*/"click"),
      /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
      /*reject_reason=*/absl::nullopt, /*is_winner=*/false));
}

TEST_F(InterestGroupPaReportUtilTest, ForEventContributionBaseValueWinningBid) {
  // Bucket should be uint128(10 * 10) + 23 = 123.
  EXPECT_EQ(kExpectedRequestWithReservedEventType,
            FillInPrivateAggregationRequest(
                CreateForEventRequestWithBucketObject(
                    CreateSignalBucket(/*scale=*/10, /*offset_value=*/23,
                                       /*is_negative=*/false),
                    /*value=*/45,
                    /*event_type=*/kReservedWin),
                /*winning_bid=*/10, /*highest_scoring_other_bid=*/1,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/true));

  // Value should be int(2.2 * 10) + 23 = 45.
  EXPECT_EQ(kExpectedRequestWithReservedEventType,
            FillInPrivateAggregationRequest(
                CreateForEventRequestWithValueObject(
                    /*bucket=*/123,
                    /*value=*/CreateSignalValue(/*scale=*/10, /*offset=*/23),
                    /*event_type=*/kReservedWin),
                /*winning_bid=*/2.2, /*highest_scoring_other_bid=*/1,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/true));
}

TEST_F(InterestGroupPaReportUtilTest,
       ForEventContributionBaseValueHighestScoringOtherBid) {
  // Bucket should be uint128(14.6 * 10) - 23 = 123.
  EXPECT_EQ(
      kExpectedRequestWithReservedEventType,
      FillInPrivateAggregationRequest(
          CreateForEventRequestWithBucketObject(
              /*bucket=*/CreateSignalBucket(
                  /*scale=*/10.0, /*offset_value=*/23, /*is_negative=*/true,
                  /*base_value=*/
                  auction_worklet::mojom::BaseValue::kHighestScoringOtherBid),
              /*value=*/45,
              /*event_type=*/kReservedWin),
          /*winning_bid=*/15, /*highest_scoring_other_bid=*/14.6,
          /*reject_reason=*/absl::nullopt, /*is_winner=*/true));

  // Value should be int(6.8 * 10) - 23 = 45.
  EXPECT_EQ(
      kExpectedRequestWithReservedEventType,
      FillInPrivateAggregationRequest(
          CreateForEventRequestWithValueObject(
              /*bucket=*/123, /*value=*/
              CreateSignalValue(
                  /*scale=*/10.0, /*offset=*/-23, /*base_value=*/
                  auction_worklet::mojom::BaseValue::kHighestScoringOtherBid),
              /*event_type=*/kReservedWin),
          /*winning_bid=*/15, /*highest_scoring_other_bid=*/6.8,
          /*reject_reason=*/absl::nullopt, /*is_winner=*/true));
}

TEST_F(InterestGroupPaReportUtilTest,
       ForEventContributionBaseValueRejectReason) {
  auction_worklet::mojom::SignalBucket signal_bucket(
      /*base_value=*/auction_worklet::mojom::BaseValue::kBidRejectReason,
      /*scale=*/39.0,
      /*offset=*/
      auction_worklet::mojom::BucketOffset::New(/*value=*/6,
                                                /*is_negative=*/false));

  // kPendingApprovalByExchange is 3. Bucket should be uint128(3 * 39) + 6 =
  // 123.
  EXPECT_EQ(
      kExpectedRequestWithReservedEventType,
      FillInPrivateAggregationRequest(
          CreateForEventRequestWithBucketObject(
              /*bucket=*/signal_bucket.Clone(), /*value=*/45,
              /*event_type=*/kReservedLoss),
          /*winning_bid=*/0, /*highest_scoring_other_bid=*/0,
          /*reject_reason=*/
          auction_worklet::mojom::RejectReason::kPendingApprovalByExchange,
          /*is_winner=*/false));

  // kInvalidBid is 1. Value should be int(1 * 39) + 6 = 45.
  EXPECT_EQ(
      kExpectedRequestWithReservedEventType,
      FillInPrivateAggregationRequest(
          CreateForEventRequestWithValueObject(
              /*bucket=*/123, /*value=*/
              CreateSignalValue(
                  /*scale=*/39.0, /*offset=*/6, /*base_value=*/
                  auction_worklet::mojom::BaseValue::kBidRejectReason),
              /*event_type=*/kReservedLoss),
          /*winning_bid=*/0, /*highest_scoring_other_bid=*/0,
          /*reject_reason=*/auction_worklet::mojom::RejectReason::kInvalidBid,
          /*is_winner=*/false));

  // kNotAvailable is also reported. kNotAvailable is 0, so bucket is 0 * 39 + 6
  PrivateAggregationRequestWithEventType expected_requests_with_event_type(
      auction_worklet::mojom::PrivateAggregationRequest::New(
          auction_worklet::mojom::AggregatableReportContribution::
              NewHistogramContribution(
                  blink::mojom::AggregatableReportHistogramContribution::New(
                      /*bucket=*/6,
                      /*value=*/45)),
          blink::mojom::AggregationServiceMode::kDefault,
          blink::mojom::DebugModeDetails::New()),
      /*event_type=*/absl::nullopt);
  EXPECT_EQ(
      std::move(expected_requests_with_event_type),
      FillInPrivateAggregationRequest(
          CreateForEventRequestWithBucketObject(
              /*bucket=*/signal_bucket.Clone(), /*value=*/45,
              /*event_type=*/kReservedLoss),
          /*winning_bid=*/2, /*highest_scoring_other_bid=*/1,
          /*reject_reason=*/auction_worklet::mojom::RejectReason::kNotAvailable,
          /*is_winner=*/false));

  // FillInPrivateAggregationRequest() should return nullptr when its
  // reject_reason parameter is empty and request's base_value is
  // kBidRejectReason.
  EXPECT_FALSE(FillInPrivateAggregationRequest(
      CreateForEventRequestWithBucketObject(
          /*bucket=*/signal_bucket.Clone(), /*value=*/45,
          /*event_type=*/kReservedLoss),
      /*winning_bid=*/0, /*highest_scoring_other_bid=*/0,
      /*reject_reason=*/absl::nullopt, /*is_winner=*/false));
}

TEST_F(InterestGroupPaReportUtilTest, ForEventContributionNegativeValue) {
  // Negative value should be clamped to 0. Worklet code should prevent an int
  // value from being negative, but worklet process can be compromised. And this
  // tests that case.
  EXPECT_EQ(CreatePrivateAggregationRequestWithEventType(
                CreateHistogramRequest(/*bucket=*/123, /*value=*/0)),
            FillInPrivateAggregationRequest(
                CreateForEventRequest(/*bucket=*/123, /*value=*/-10,
                                      /*event_type=*/kReservedAlways),
                /*winning_bid=*/1, /*highest_scoring_other_bid=*/2,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/false));

  // Calculated negative value should be clamped to 0.
  EXPECT_EQ(
      CreatePrivateAggregationRequestWithEventType(
          CreateHistogramRequest(/*bucket=*/123, /*value=*/0)),
      FillInPrivateAggregationRequest(
          CreateForEventRequestWithValueObject(
              /*bucket=*/123, /*value=*/
              CreateSignalValue(
                  /*scale=*/-10.0, /*offset=*/0, /*base_value=*/
                  auction_worklet::mojom::BaseValue::kHighestScoringOtherBid),
              /*event_type=*/kReservedWin),
          /*winning_bid=*/1, /*highest_scoring_other_bid=*/6.8,
          /*reject_reason=*/absl::nullopt, /*is_winner=*/true));
}

TEST_F(InterestGroupPaReportUtilTest, ForEventContributionNoScaleOrOffset) {
  // No scale is provided to bucket.
  auction_worklet::mojom::SignalBucket bucket;
  bucket.base_value = auction_worklet::mojom::BaseValue::kWinningBid;

  // Default scale is 1.0. Bucket should be 123 * 1.0 = 123.
  EXPECT_EQ(kExpectedRequestWithReservedEventType,
            FillInPrivateAggregationRequest(
                CreateForEventRequestWithBucketObject(
                    /*bucket=*/bucket.Clone(), /*value=*/45,
                    /*event_type=*/kReservedWin),
                /*winning_bid=*/123, /*highest_scoring_other_bid=*/1,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/true));

  // No scale or offset are provided to value.
  auction_worklet::mojom::SignalValue value;
  value.base_value = auction_worklet::mojom::BaseValue::kWinningBid;

  // Default scale is 1.0 and default offset is 0. Value should be 45 * 1.0 + 0
  EXPECT_EQ(kExpectedRequestWithReservedEventType,
            FillInPrivateAggregationRequest(
                CreateForEventRequestWithValueObject(
                    /*bucket=*/123, value.Clone(),
                    /*event_type=*/kReservedWin),
                /*winning_bid=*/45, /*highest_scoring_other_bid=*/1,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/true));
}

TEST_F(InterestGroupPaReportUtilTest, ForEventContributionZeroScale) {
  auction_worklet::mojom::SignalBucket bucket(
      /*base_value=*/auction_worklet::mojom::BaseValue::kWinningBid,
      /*scale=*/0,
      /*offset=*/nullptr);

  // Bucket should be 123 * 0
  EXPECT_EQ(CreatePrivateAggregationRequestWithEventType(
                CreateHistogramRequest(/*bucket=*/0, /*value=*/45)),
            FillInPrivateAggregationRequest(
                CreateForEventRequestWithBucketObject(
                    /*bucket=*/bucket.Clone(),
                    /*value=*/45,
                    /*event_type=*/kReservedWin),
                /*winning_bid=*/123, /*highest_scoring_other_bid=*/1,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/true));

  auction_worklet::mojom::SignalValue value(
      /*base_value=*/auction_worklet::mojom::BaseValue::kWinningBid,
      /*scale=*/0,
      /*offset=*/0);

  // Value should be 45 * 0 + 0
  EXPECT_EQ(CreatePrivateAggregationRequestWithEventType(
                CreateHistogramRequest(/*bucket=*/123, /*value=*/0)),
            FillInPrivateAggregationRequest(
                CreateForEventRequestWithValueObject(
                    /*bucket=*/123,
                    /*value=*/value.Clone(),
                    /*event_type=*/kReservedWin),
                /*winning_bid=*/45, /*highest_scoring_other_bid=*/1,
                /*reject_reason=*/absl::nullopt, /*is_winner=*/true));
}

TEST_F(InterestGroupPaReportUtilTest, ForEventContributionCalculateBucket) {
  struct {
    double base;
    double scale;
    absl::uint128 offset;
    bool offset_is_negative;
    absl::optional<absl::uint128> expected_bucket;
  } test_cases[] = {
      // Overflow in base*scale. It shouldn't matter whether the base or scale
      // or the two combined cause the overflow. Overflows due to the value is
      // too big should go to Uint128Max. Overflows due to the value is negative
      // go to 0.
      // 1 * pow(2,128) => absl::Uint128Max()
      {1, std::ldexp(1, 128), 0, false, absl::Uint128Max()},
      // pow(2,128) * 1 => absl::Uint128Max()
      {std::ldexp(1, 128), 1, 0, false, absl::Uint128Max()},
      // 2 * pow(2,127)=> absl::Uint128Max()
      {2, std::ldexp(1, 127), 0, false, absl::Uint128Max()},
      // pow(2,127) * 2 => absl::Uint128Max()
      {std::ldexp(1, 127), 2, 0, false, absl::Uint128Max()},
      // -1 * pow(2,128) => 0
      {-1, std::ldexp(1, 128), 0, false, 0},
      // pow(2,128) * -1 => 0
      {std::ldexp(1, 128), -1, 0, false, 0},
      // 2 * -pow(2,127) => 0
      {2, -std::ldexp(1, 127), 0, false, 0},
      // -pow(2,127) * 2 => 0
      {-std::ldexp(1, 127), 2, 0, false, 0},
      // -1 * -pow(2,128) => absl::Uint128Max()
      {-1, -std::ldexp(1, 128), 0, false, absl::Uint128Max()},
      // -pow(2,128) * -1 => absl::Uint128Max()
      {-std::ldexp(1, 128), -1, 0, false, absl::Uint128Max()},
      // inf * 1 => absl::Uint128Max()
      {std::numeric_limits<double>::infinity(), 1, 0, false,
       absl::Uint128Max()},
      // 1 * inf => absl::Uint128Max()
      {1, std::numeric_limits<double>::infinity(), 0, false,
       absl::Uint128Max()},
      // -inf * 1 => 0
      {-std::numeric_limits<double>::infinity(), 1, 0, false, 0},
      // -1 * inf => 0
      {-1, std::numeric_limits<double>::infinity(), 0, false, 0},
      // NaN * 1 => absl::nullopt
      {std::numeric_limits<double>::quiet_NaN(), 1, 0, false, absl::nullopt},
      // 1 * NaN => absl::nullopt
      {1, std::numeric_limits<double>::quiet_NaN(), 0, false, absl::nullopt},
      // NaN * -1 => absl::nullopt
      {std::numeric_limits<double>::quiet_NaN(), -1, 0, false, absl::nullopt},
      // -1 * NaN => absl::nullopt
      {-1, std::numeric_limits<double>::quiet_NaN(), 0, false, absl::nullopt},

      // Overflow from adding/subtracting offset.
      // 1 * 1 + absl::Uint128Max() => absl::Uint128Max()
      {1, 1, absl::Uint128Max(), false, absl::Uint128Max()},
      // -1 * 1 - 1 => 0
      {-1, 1, 1, true, 0},
      // 1 * 1 - 2 => 0
      {-1, 1, 2, true, 0},

      // Result does not overflow.
      // 1.9 * 2.0 - 1 => 2
      {1.9, 2.0, 1, true, 2},
      // 1 * 2 - 1 => 1
      {1, 2, 1, true, 1},
      // 1 * -1 + 2 => 1
      {1, -1, 2, false, 1},
      // 1 * 1 + 1 => 2
      {1, 1, 1, false, 2},
  };
  for (const auto& test_case : test_cases) {
    absl::optional<PrivateAggregationRequestWithEventType> request =
        FillInPrivateAggregationRequest(
            CreateForEventRequestWithBucketObject(
                CreateSignalBucket(test_case.scale, test_case.offset,
                                   test_case.offset_is_negative),
                /*value=*/45,
                /*event_type=*/kReservedAlways),
            /*winning_bid=*/test_case.base,
            /*highest_scoring_other_bid=*/0,
            /*reject_reason=*/absl::nullopt, /*is_winner=*/true);
    if (test_case.expected_bucket.has_value()) {
      ASSERT_TRUE(request.has_value());
      EXPECT_EQ(CreatePrivateAggregationRequestWithEventType(
                    CreateHistogramRequest(test_case.expected_bucket.value(),
                                           /*value=*/45)),
                request.value());
    } else {
      EXPECT_FALSE(request);
    }
  }
}

TEST_F(InterestGroupPaReportUtilTest, ForEventContributionCalculateValue) {
  struct {
    double base;
    double scale;
    int32_t offset;
    absl::optional<int32_t> expected_value;
  } test_cases[] = {
      // Result overflows.
      // INT32_MAX * 1 + 1 => INT32_MAX
      {1, INT32_MAX, INT32_MAX, INT32_MAX},
      // 2 * INT32_MAX + 1 => INT32_MAX
      {2, INT32_MAX, INT32_MAX, INT32_MAX},
      // 1 * 1 + INT32_MAX => INT32_MAX
      {1, 1, INT32_MAX, INT32_MAX},
      // INT32_MIN * 1 - 1 => 0
      {INT32_MIN, 1, -1, 0},
      // INT32_MAX * -1 - 1 => 0
      {INT32_MAX, -1, -1, 0},
      // INT32_MIN * -1 + 0 => INT32_MAX
      {INT32_MIN, -1, 0, INT32_MAX},

      // inf * 1 => INT32_MAX
      {std::numeric_limits<double>::infinity(), 1, 0, INT32_MAX},
      // 1 * inf => INT32_MAX
      {1, std::numeric_limits<double>::infinity(), 0, INT32_MAX},
      // -inf * 1 => 0
      {-std::numeric_limits<double>::infinity(), 1, 0, 0},
      // -1 * inf => 0
      {-1, std::numeric_limits<double>::infinity(), 0, 0},
      // NaN * 1 => absl::nullopt
      {std::numeric_limits<double>::quiet_NaN(), 1, 0, absl::nullopt},
      // 1 * NaN => absl::nullopt
      {1, std::numeric_limits<double>::quiet_NaN(), 0, absl::nullopt},
      // NaN * -1 => absl::nullopt
      {std::numeric_limits<double>::quiet_NaN(), -1, 0, absl::nullopt},
      // -1 * NaN => absl::nullopt
      {-1, std::numeric_limits<double>::quiet_NaN(), 0, absl::nullopt},

      // Result does not overflow. Double result will be truncated to int32 if
      // necessary.
      // INT32_MIN * -1 - 2 => INT32_MAX - 1
      {INT32_MIN, -1, -2, INT32_MAX - 1},
      // 1.9 * 2.0 - 1 => 2
      {1.9, 2.0, -1, 2},
      // 1.9 * -2.0 + 2 => 0
      {1.9, -2.0, 2, 0},
      // 1.9 * -2.0 + 4 => 0
      {1.9, -2.0, 4, 0},
  };
  for (const auto& test_case : test_cases) {
    absl::optional<PrivateAggregationRequestWithEventType> request =
        FillInPrivateAggregationRequest(
            CreateForEventRequestWithValueObject(
                /*bucket=*/123,
                CreateSignalValue(test_case.scale, test_case.offset),
                /*event_type=*/kReservedAlways),
            /*winning_bid=*/test_case.base,
            /*highest_scoring_other_bid=*/0,
            /*reject_reason=*/absl::nullopt, /*is_winner=*/true);

    if (test_case.expected_value.has_value()) {
      ASSERT_TRUE(request.has_value());
      EXPECT_EQ(
          CreatePrivateAggregationRequestWithEventType(CreateHistogramRequest(
              /*bucket=*/123, test_case.expected_value.value())),
          request.value());
    } else {
      EXPECT_FALSE(request);
    }
  }
}

}  // namespace content

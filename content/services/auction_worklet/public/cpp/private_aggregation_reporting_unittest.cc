// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/public/cpp/private_aggregation_reporting.h"

#include <stdint.h>

#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"

namespace auction_worklet {
namespace {

class PaReportingTest : public testing::Test {
 public:
  PaReportingTest() = default;
  ~PaReportingTest() override = default;

  // Using reserved.once as the event.
  const mojom::PrivateAggregationRequestPtr kWithReservedOnce =
      mojom::PrivateAggregationRequest::New(
          mojom::AggregatableReportContribution::NewForEventContribution(
              mojom::AggregatableReportForEventContribution::New(
                  mojom::ForEventSignalBucket::NewIdBucket(1),
                  mojom::ForEventSignalValue::NewIntValue(2),
                  /*filtering_id=*/std::nullopt,
                  mojom::EventType::NewReserved(
                      mojom::ReservedEventType::kReservedOnce))),
          blink::mojom::AggregationServiceMode::kDefault,
          blink::mojom::DebugModeDetails::New());

  // Using reserved.always as the event.
  const mojom::PrivateAggregationRequestPtr kWithReservedAlways =
      mojom::PrivateAggregationRequest::New(
          mojom::AggregatableReportContribution::NewForEventContribution(
              mojom::AggregatableReportForEventContribution::New(
                  mojom::ForEventSignalBucket::NewIdBucket(1),
                  mojom::ForEventSignalValue::NewIntValue(2),
                  /*filtering_id=*/std::nullopt,
                  mojom::EventType::NewReserved(
                      mojom::ReservedEventType::kReservedAlways))),
          blink::mojom::AggregationServiceMode::kDefault,
          blink::mojom::DebugModeDetails::New());

  // Using a custom event.
  const mojom::PrivateAggregationRequestPtr kNonReserved =
      mojom::PrivateAggregationRequest::New(
          mojom::AggregatableReportContribution::NewForEventContribution(
              mojom::AggregatableReportForEventContribution::New(
                  mojom::ForEventSignalBucket::NewIdBucket(1),
                  mojom::ForEventSignalValue::NewIntValue(2),
                  /*filtering_id=*/std::nullopt,
                  mojom::EventType::NewNonReserved("event_type"))),
          blink::mojom::AggregationServiceMode::kDefault,
          blink::mojom::DebugModeDetails::New());

  // Using kWinningBid base_value for bucket and value.
  const mojom::PrivateAggregationRequestPtr kWithOldBaseValues =
      mojom::PrivateAggregationRequest::New(
          mojom::AggregatableReportContribution::NewForEventContribution(
              mojom::AggregatableReportForEventContribution::New(
                  mojom::ForEventSignalBucket::NewSignalBucket(
                      mojom::SignalBucket::New(mojom::BaseValue::kWinningBid,
                                               1.0,
                                               mojom::BucketOffsetPtr())),
                  mojom::ForEventSignalValue::NewSignalValue(
                      mojom::SignalValue::New(mojom::BaseValue::kWinningBid,
                                              1.0,
                                              0)),
                  /*filtering_id=*/std::nullopt,
                  mojom::EventType::NewNonReserved("event_type"))),
          blink::mojom::AggregationServiceMode::kDefault,
          blink::mojom::DebugModeDetails::New());

  // Using kAverageCodeFetchTime for value.
  const mojom::PrivateAggregationRequestPtr kWithNewValueBaseValue =
      mojom::PrivateAggregationRequest::New(
          mojom::AggregatableReportContribution::NewForEventContribution(
              mojom::AggregatableReportForEventContribution::New(
                  mojom::ForEventSignalBucket::NewSignalBucket(
                      mojom::SignalBucket::New(mojom::BaseValue::kWinningBid,
                                               1.0,
                                               mojom::BucketOffsetPtr())),
                  mojom::ForEventSignalValue::NewSignalValue(
                      mojom::SignalValue::New(
                          mojom::BaseValue::kAverageCodeFetchTime,
                          1.0,
                          0)),
                  /*filtering_id=*/std::nullopt,
                  mojom::EventType::NewNonReserved("event_type"))),
          blink::mojom::AggregationServiceMode::kDefault,
          blink::mojom::DebugModeDetails::New());

  // Using kAverageCodeFetchTime for bucket.
  const mojom::PrivateAggregationRequestPtr kWithNewBucketBaseValue =
      mojom::PrivateAggregationRequest::New(
          mojom::AggregatableReportContribution::NewForEventContribution(
              mojom::AggregatableReportForEventContribution::New(
                  mojom::ForEventSignalBucket::NewSignalBucket(
                      mojom::SignalBucket::New(
                          mojom::BaseValue::kAverageCodeFetchTime,
                          1.0,
                          mojom::BucketOffsetPtr())),
                  mojom::ForEventSignalValue::NewSignalValue(
                      mojom::SignalValue::New(mojom::BaseValue::kWinningBid,
                                              1.0,
                                              0)),
                  /*filtering_id=*/std::nullopt,
                  mojom::EventType::NewNonReserved("event_type"))),
          blink::mojom::AggregationServiceMode::kDefault,
          blink::mojom::DebugModeDetails::New());

  // Just a raw histogram, not conditional on an event.
  const mojom::PrivateAggregationRequestPtr kNonEvent =
      mojom::PrivateAggregationRequest::New(
          mojom::AggregatableReportContribution::NewHistogramContribution(
              blink::mojom::AggregatableReportHistogramContribution::New(
                  /*bucket=*/42,
                  /*value=*/24,
                  /*filtering_id=*/std::nullopt)),
          blink::mojom::AggregationServiceMode::kDefault,
          blink::mojom::DebugModeDetails::New());

  // Using kWinningBid base_value for bucket and value.
  const mojom::PrivateAggregationRequestPtr kWinningBid =
      mojom::PrivateAggregationRequest::New(
          mojom::AggregatableReportContribution::NewForEventContribution(
              mojom::AggregatableReportForEventContribution::New(
                  mojom::ForEventSignalBucket::NewSignalBucket(
                      mojom::SignalBucket::New(mojom::BaseValue::kWinningBid,
                                               1.0,
                                               mojom::BucketOffsetPtr())),
                  mojom::ForEventSignalValue::NewSignalValue(
                      mojom::SignalValue::New(mojom::BaseValue::kWinningBid,
                                              1.0,
                                              0)),
                  /*filtering_id=*/std::nullopt,
                  mojom::EventType::NewNonReserved("event_type"))),
          blink::mojom::AggregationServiceMode::kDefault,
          blink::mojom::DebugModeDetails::New());

  // Using kRejectReason for value.
  const mojom::PrivateAggregationRequestPtr kWithRejectReasonValue =
      mojom::PrivateAggregationRequest::New(
          mojom::AggregatableReportContribution::NewForEventContribution(
              mojom::AggregatableReportForEventContribution::New(
                  mojom::ForEventSignalBucket::NewIdBucket(1),
                  mojom::ForEventSignalValue::NewSignalValue(
                      mojom::SignalValue::New(
                          mojom::BaseValue::kBidRejectReason,
                          1.0,
                          0)),
                  /*filtering_id=*/std::nullopt,
                  mojom::EventType::NewNonReserved("event_type"))),
          blink::mojom::AggregationServiceMode::kDefault,
          blink::mojom::DebugModeDetails::New());

  // Using kRejectReason for bucket.
  const mojom::PrivateAggregationRequestPtr kWithRejectReasonBucket =
      mojom::PrivateAggregationRequest::New(
          mojom::AggregatableReportContribution::NewForEventContribution(
              mojom::AggregatableReportForEventContribution::New(
                  mojom::ForEventSignalBucket::NewSignalBucket(
                      mojom::SignalBucket::New(
                          mojom::BaseValue::kBidRejectReason,
                          1.0,
                          mojom::BucketOffsetPtr())),
                  mojom::ForEventSignalValue::NewIntValue(2),
                  /*filtering_id=*/std::nullopt,
                  mojom::EventType::NewNonReserved("event_type"))),
          blink::mojom::AggregationServiceMode::kDefault,
          blink::mojom::DebugModeDetails::New());
};

TEST_F(PaReportingTest,
       IsValidPrivateAggregationRequestForAdditionalExtensions) {
  EXPECT_TRUE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kWithReservedOnce, /*additional_extensions_allowed=*/true));
  EXPECT_FALSE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kWithReservedOnce, /*additional_extensions_allowed=*/false));

  EXPECT_TRUE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kWithReservedAlways, /*additional_extensions_allowed=*/true));
  EXPECT_TRUE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kWithReservedAlways, /*additional_extensions_allowed=*/false));

  EXPECT_TRUE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kNonReserved, /*additional_extensions_allowed=*/true));
  EXPECT_TRUE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kNonReserved, /*additional_extensions_allowed=*/false));

  EXPECT_TRUE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kNonEvent, /*additional_extensions_allowed=*/true));
  EXPECT_TRUE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kNonEvent, /*additional_extensions_allowed=*/false));

  EXPECT_TRUE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kWithOldBaseValues, /*additional_extensions_allowed=*/true));
  EXPECT_TRUE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kWithOldBaseValues, /*additional_extensions_allowed=*/false));

  EXPECT_TRUE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kWithNewValueBaseValue, /*additional_extensions_allowed=*/true));
  EXPECT_FALSE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kWithNewValueBaseValue, /*additional_extensions_allowed=*/false));

  EXPECT_TRUE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kWithNewBucketBaseValue, /*additional_extensions_allowed=*/true));
  EXPECT_FALSE(IsValidPrivateAggregationRequestForAdditionalExtensions(
      *kWithNewBucketBaseValue, /*additional_extensions_allowed=*/false));
}

TEST_F(PaReportingTest, RequiresAdditionalExtensions) {
  EXPECT_FALSE(RequiresAdditionalExtensions(mojom::BaseValue::kWinningBid));
  EXPECT_FALSE(
      RequiresAdditionalExtensions(mojom::BaseValue::kHighestScoringOtherBid));
  EXPECT_FALSE(RequiresAdditionalExtensions(mojom::BaseValue::kScriptRunTime));
  EXPECT_FALSE(
      RequiresAdditionalExtensions(mojom::BaseValue::kSignalsFetchTime));
  EXPECT_FALSE(
      RequiresAdditionalExtensions(mojom::BaseValue::kBidRejectReason));
  EXPECT_TRUE(RequiresAdditionalExtensions(
      mojom::BaseValue::kParticipatingInterestGroupCount));
  EXPECT_TRUE(
      RequiresAdditionalExtensions(mojom::BaseValue::kAverageCodeFetchTime));
}

TEST_F(PaReportingTest, HasKAnonFailureComponent) {
  EXPECT_FALSE(HasKAnonFailureComponent(*kNonEvent));
  EXPECT_FALSE(HasKAnonFailureComponent(*kNonReserved));
  EXPECT_FALSE(HasKAnonFailureComponent(*kWithReservedAlways));
  EXPECT_FALSE(HasKAnonFailureComponent(*kWinningBid));
  EXPECT_TRUE(HasKAnonFailureComponent(*kWithRejectReasonValue));
  EXPECT_TRUE(HasKAnonFailureComponent(*kWithRejectReasonBucket));
}

}  // namespace
}  // namespace auction_worklet

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
}

}  // namespace
}  // namespace auction_worklet

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/attribution_reporting_to_mojom.h"

#include "services/network/public/mojom/attribution.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_attribution_reporting_request_options.h"

namespace blink {
namespace {

using ::network::mojom::AttributionReportingEligibility;

TEST(AttributionReportingToMojomTest, Convert) {
  const struct {
    bool event_source_eligible;
    bool trigger_eligible;
    AttributionReportingEligibility expected_result;
  } kTestCases[] = {
      {false, false, AttributionReportingEligibility::kEmpty},
      {false, true, AttributionReportingEligibility::kTrigger},
      {true, false, AttributionReportingEligibility::kEventSource},
      {true, true, AttributionReportingEligibility::kEventSourceOrTrigger},
  };

  for (const auto& test_case : kTestCases) {
    auto* options = AttributionReportingRequestOptions::Create();
    options->setEventSourceEligible(test_case.event_source_eligible);
    options->setTriggerEligible(test_case.trigger_eligible);

    EXPECT_EQ(test_case.expected_result,
              ConvertAttributionReportingRequestOptionsToMojom(*options));
  }
}

}  // namespace
}  // namespace blink

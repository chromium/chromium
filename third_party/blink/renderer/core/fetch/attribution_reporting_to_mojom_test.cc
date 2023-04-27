// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/attribution_reporting_to_mojom.h"

#include "base/strings/stringprintf.h"
#include "services/network/public/mojom/attribution.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_attribution_reporting_request_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {
namespace {

using ::network::mojom::AttributionReportingEligibility;

ScopedNullExecutionContext MakeExecutionContext(bool has_permission) {
  ParsedPermissionsPolicy parsed_policy;

  if (has_permission) {
    AllowFeatureEverywhere(
        mojom::blink::PermissionsPolicyFeature::kAttributionReporting,
        parsed_policy);
  } else {
    DisallowFeature(
        mojom::blink::PermissionsPolicyFeature::kAttributionReporting,
        parsed_policy);
  }

  ScopedNullExecutionContext execution_context;

  auto origin = SecurityOrigin::CreateFromString("https://example.test");

  execution_context.GetExecutionContext()
      .GetSecurityContext()
      .SetPermissionsPolicy(PermissionsPolicy::CreateFromParsedPolicy(
          parsed_policy, origin->ToUrlOrigin()));

  return execution_context;
}

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
    SCOPED_TRACE(base::StringPrintf(
        "event_source_eligible=%d,trigger_eligible=%d",
        test_case.event_source_eligible, test_case.trigger_eligible));

    auto* options = AttributionReportingRequestOptions::Create();
    options->setEventSourceEligible(test_case.event_source_eligible);
    options->setTriggerEligible(test_case.trigger_eligible);

    {
      V8TestingScope scope;
      auto execution_context = MakeExecutionContext(/*has_permission=*/true);

      EXPECT_EQ(test_case.expected_result,
                ConvertAttributionReportingRequestOptionsToMojom(
                    *options, execution_context.GetExecutionContext(),
                    scope.GetExceptionState()));

      EXPECT_FALSE(scope.GetExceptionState().HadException());
    }

    {
      V8TestingScope scope;
      auto execution_context = MakeExecutionContext(/*has_permission=*/false);

      EXPECT_EQ(AttributionReportingEligibility::kUnset,
                ConvertAttributionReportingRequestOptionsToMojom(
                    *options, execution_context.GetExecutionContext(),
                    scope.GetExceptionState()));

      EXPECT_TRUE(scope.GetExceptionState().HadException());
    }
  }
}

}  // namespace
}  // namespace blink

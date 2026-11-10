// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/public/metrics.h"

#import "base/strings/strcat.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/time/time.h"
#import "components/actor/public/mojom/actor_types.mojom-shared.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

namespace {

// Test case defining an `ActorJavaScriptCall` and its expected UMA histogram
// names.
struct HistogramNameTestCase {
  const char* test_name;
  ActorJavaScriptCall call;
  std::string_view expected_duration_histogram;
  std::string_view expected_result_code_histogram;
};

// Value-parameterized test fixture verifying UMA histogram name mapping for
// every `ActorJavaScriptCall` variant.
class MetricsHistogramNamesTest
    : public PlatformTest,
      public ::testing::WithParamInterface<HistogramNameTestCase> {};

}  // namespace

using MetricsTest = PlatformTest;

// Tests that `GetJavaScriptExecutionDurationHistogramName` and
// `GetJavaScriptResultCodeHistogramName` return the expected histogram names
// for each `ActorJavaScriptCall` variant.
TEST_P(MetricsHistogramNamesTest, ReturnsExpectedHistogramNames) {
  const HistogramNameTestCase& test_case = GetParam();
  EXPECT_EQ(GetJavaScriptExecutionDurationHistogramName(test_case.call),
            test_case.expected_duration_histogram);
  EXPECT_EQ(GetJavaScriptResultCodeHistogramName(test_case.call),
            test_case.expected_result_code_histogram);
}

INSTANTIATE_TEST_SUITE_P(
    AllCalls,
    MetricsHistogramNamesTest,
    ::testing::Values(
        HistogramNameTestCase{
            "ActionTarget", ActorJavaScriptCall::kActionTarget,
            "IOS.Actor.Tools.JavaScriptExecutionDuration.ActionTarget",
            "IOS.Actor.Tools.JavaScriptResultCode.ActionTarget"},
        HistogramNameTestCase{
            "AttemptFormFilling", ActorJavaScriptCall::kAttemptFormFilling,
            "IOS.Actor.Tools.JavaScriptExecutionDuration.AttemptFormFilling",
            "IOS.Actor.Tools.JavaScriptResultCode.AttemptFormFilling"},
        HistogramNameTestCase{
            "Click", ActorJavaScriptCall::kClick,
            "IOS.Actor.Tools.JavaScriptExecutionDuration.Click",
            "IOS.Actor.Tools.JavaScriptResultCode.Click"},
        HistogramNameTestCase{
            "DragAndRelease", ActorJavaScriptCall::kDragAndRelease,
            "IOS.Actor.Tools.JavaScriptExecutionDuration.DragAndRelease",
            "IOS.Actor.Tools.JavaScriptResultCode.DragAndRelease"},
        HistogramNameTestCase{
            "Scroll", ActorJavaScriptCall::kScroll,
            "IOS.Actor.Tools.JavaScriptExecutionDuration.Scroll",
            "IOS.Actor.Tools.JavaScriptResultCode.Scroll"},
        HistogramNameTestCase{
            "ScrollTo", ActorJavaScriptCall::kScrollTo,
            "IOS.Actor.Tools.JavaScriptExecutionDuration.ScrollTo",
            "IOS.Actor.Tools.JavaScriptResultCode.ScrollTo"},
        HistogramNameTestCase{
            "Select", ActorJavaScriptCall::kSelect,
            "IOS.Actor.Tools.JavaScriptExecutionDuration.Select",
            "IOS.Actor.Tools.JavaScriptResultCode.Select"},
        HistogramNameTestCase{
            "Type", ActorJavaScriptCall::kType,
            "IOS.Actor.Tools.JavaScriptExecutionDuration.Type",
            "IOS.Actor.Tools.JavaScriptResultCode.Type"},
        HistogramNameTestCase{"TypeSubmitWithEnterFallback",
                              ActorJavaScriptCall::kTypeSubmitWithEnterFallback,
                              "IOS.Actor.Tools.JavaScriptExecutionDuration."
                              "TypeSubmitWithEnterFallback",
                              "IOS.Actor.Tools.JavaScriptResultCode."
                              "TypeSubmitWithEnterFallback"}),
    [](const ::testing::TestParamInfo<HistogramNameTestCase>& info) {
      return info.param.test_name;
    });

// Tests that `RecordJavaScriptExecutionDuration` records samples to the
// expected duration histogram without emitting to the result code histogram.
TEST_F(MetricsTest, RecordJavaScriptExecutionDuration) {
  base::HistogramTester histogram_tester;

  RecordJavaScriptExecutionDuration(ActorJavaScriptCall::kClick,
                                    base::Milliseconds(42));

  histogram_tester.ExpectUniqueTimeSample(
      "IOS.Actor.Tools.JavaScriptExecutionDuration.Click",
      base::Milliseconds(42), 1);
  histogram_tester.ExpectTotalCount(
      "IOS.Actor.Tools.JavaScriptResultCode.Click", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.Actor.Tools.JavaScriptExecutionDuration.ActionTarget", 0);
}

// Tests that `RecordJavaScriptResultCode` records samples to the expected
// result code histogram without emitting to the duration histogram.
TEST_F(MetricsTest, RecordJavaScriptResultCode) {
  base::HistogramTester histogram_tester;

  RecordJavaScriptResultCode(ActorJavaScriptCall::kClick,
                             mojom::ActionResultCode::kOk);
  RecordJavaScriptResultCode(ActorJavaScriptCall::kClick,
                             mojom::ActionResultCode::kToolTimeout);

  histogram_tester.ExpectBucketCount(
      "IOS.Actor.Tools.JavaScriptResultCode.Click",
      mojom::ActionResultCode::kOk, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.Actor.Tools.JavaScriptResultCode.Click",
      mojom::ActionResultCode::kToolTimeout, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.Actor.Tools.JavaScriptResultCode.Click", 2);
  histogram_tester.ExpectTotalCount(
      "IOS.Actor.Tools.JavaScriptExecutionDuration.Click", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.Actor.Tools.JavaScriptResultCode.ActionTarget", 0);
}

// Tests that `RecordToolTimings` records execution and page stabilization
// durations to the expected histograms.
TEST_F(MetricsTest, RecordToolTimings) {
  base::HistogramTester histogram_tester;

  RecordToolTimings(ToolType::kClick, base::Milliseconds(120),
                    base::Milliseconds(340));

  histogram_tester.ExpectUniqueTimeSample(
      base::StrCat(
          {kIOSActorToolsExecutionDurationPrefixMetricName, "ClickTool"}),
      base::Milliseconds(120), 1);
  histogram_tester.ExpectUniqueTimeSample(
      base::StrCat(
          {kIOSActorToolsPageStabilizationPrefixMetricName, "ClickTool"}),
      base::Milliseconds(340), 1);
}

// Tests that `RecordToolTimings` skips recording histograms when `tool_type` is
// `ToolType::kUnknown`.
TEST_F(MetricsTest, RecordToolTimingsUnknownTool) {
  base::HistogramTester histogram_tester;

  RecordToolTimings(ToolType::kUnknown, base::Milliseconds(120),
                    base::Milliseconds(340));

  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kIOSActorToolsExecutionDurationPrefixMetricName, "UnknownTool"}),
      0);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kIOSActorToolsPageStabilizationPrefixMetricName, "UnknownTool"}),
      0);
}

// Tests that `RecordToolActionResultCode` records both aggregate and per-tool
// action result codes.
TEST_F(MetricsTest, RecordToolActionResultCode) {
  base::HistogramTester histogram_tester;

  RecordToolActionResultCode(ToolType::kClick,
                             ToolExecutionResult(mojom::ActionResultCode::kOk));

  histogram_tester.ExpectUniqueSample(
      kIOSActorExecutionEngineActionResultCodeMetricName,
      mojom::ActionResultCode::kOk, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kIOSActorToolsActionResultCodePrefixMetricName, "ClickTool"}),
      mojom::ActionResultCode::kOk, 1);
}

// Tests that `RecordToolActionResultCode` records aggregate action result codes
// and skips per-tool histograms when `tool_type` is `ToolType::kUnknown`.
TEST_F(MetricsTest, RecordToolActionResultCodeUnknownTool) {
  base::HistogramTester histogram_tester;

  RecordToolActionResultCode(
      ToolType::kUnknown,
      ToolExecutionResult(mojom::ActionResultCode::kToolUnknown));

  histogram_tester.ExpectUniqueSample(
      kIOSActorExecutionEngineActionResultCodeMetricName,
      mojom::ActionResultCode::kToolUnknown, 1);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kIOSActorToolsActionResultCodePrefixMetricName, "UnknownTool"}),
      0);
}

// Tests that `RecordActionTargetType` records the target type to the
// expected histogram.
TEST_F(MetricsTest, RecordActionTargetType) {
  base::HistogramTester histogram_tester;

  RecordActionTargetType(ActionTargetType::kDomNodeId);
  RecordActionTargetType(ActionTargetType::kCoordinate);

  histogram_tester.ExpectBucketCount(kIOSActorToolsActionTargetTypeMetricName,
                                     ActionTargetType::kDomNodeId, 1);
  histogram_tester.ExpectBucketCount(kIOSActorToolsActionTargetTypeMetricName,
                                     ActionTargetType::kCoordinate, 1);
  histogram_tester.ExpectTotalCount(kIOSActorToolsActionTargetTypeMetricName,
                                    2);
}

// Tests that `RecordActionResultCode` records the action result code.
TEST_F(MetricsTest, RecordActionResultCode) {
  base::HistogramTester histogram_tester;

  RecordActionResultCode(mojom::ActionResultCode::kOk);
  RecordActionResultCode(mojom::ActionResultCode::kUrlBlocked);

  histogram_tester.ExpectBucketCount(
      kIOSActorExecutionEngineActionResultCodeMetricName,
      mojom::ActionResultCode::kOk, 1);
  histogram_tester.ExpectBucketCount(
      kIOSActorExecutionEngineActionResultCodeMetricName,
      mojom::ActionResultCode::kUrlBlocked, 1);
  histogram_tester.ExpectTotalCount(
      kIOSActorExecutionEngineActionResultCodeMetricName, 2);
}

// Tests that `RecordTargetFrameResolutionResult` records the result code and
// iframe depth when provided.
TEST_F(MetricsTest, RecordTargetFrameResolutionResult) {
  base::HistogramTester histogram_tester;

  RecordTargetFrameResolutionResult(mojom::ActionResultCode::kOk,
                                    /*iframe_depth=*/2);
  RecordTargetFrameResolutionResult(mojom::ActionResultCode::kToolTimeout);
  RecordTargetFrameResolutionResult(mojom::ActionResultCode::kOk);

  histogram_tester.ExpectBucketCount(
      kIOSActorToolsTargetFrameResolutionResultMetricName,
      mojom::ActionResultCode::kOk, 2);
  histogram_tester.ExpectBucketCount(
      kIOSActorToolsTargetFrameResolutionResultMetricName,
      mojom::ActionResultCode::kToolTimeout, 1);
  histogram_tester.ExpectTotalCount(
      kIOSActorToolsTargetFrameResolutionResultMetricName, 3);

  histogram_tester.ExpectUniqueSample(kIOSActorToolsTargetIframeDepthMetricName,
                                      2, 1);
}

}  // namespace actor

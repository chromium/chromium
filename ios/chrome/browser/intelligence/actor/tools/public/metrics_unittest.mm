// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/public/metrics.h"

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
            "IOS.Actor.Tools.JavaScriptResultCode.Type"}),
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

}  // namespace actor

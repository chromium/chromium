// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_util.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/timer/elapsed_timer.h"
#import "base/values.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/metrics.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

namespace {

// Translates integer error codes to sample `mojom::ActionResultCode` values
// for testing.
mojom::ActionResultCode FakeTranslator(int code) {
  switch (code) {
    case 0:
      return mojom::ActionResultCode::kOk;
    case 1:
      return mojom::ActionResultCode::kCoordinatesOutOfBounds;
    default:
      return mojom::ActionResultCode::kElementDisabled;
  }
}

using ActorToolJavaScriptFeatureUtilTest = PlatformTest;

// Tests that a null result (such as when a JavaScript call times out) records
// `kToolTimeout` to the result code histogram, does not record execution
// duration, and sets `requires_page_stabilization` to false.
TEST_F(ActorToolJavaScriptFeatureUtilTest, NullResultRecordsTimeout) {
  base::HistogramTester histogram_tester;
  base::ElapsedTimer timer;

  ToolExecutionResult result = ParseJavaScriptResultWithResultCode(
      ActorJavaScriptCall::kClick, &FakeTranslator, /*result=*/nullptr, timer);

  EXPECT_FALSE(result.IsOk());
  EXPECT_FALSE(result.requires_page_stabilization());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kToolTimeout);
  histogram_tester.ExpectUniqueSample(
      GetJavaScriptResultCodeHistogramName(ActorJavaScriptCall::kClick),
      mojom::ActionResultCode::kToolTimeout, 1);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptExecutionDurationHistogramName(ActorJavaScriptCall::kClick),
      0);
}

// Tests that a non-dictionary result returns
// `InternalToolErrorCode::kJavascriptFeatureGotInvalidResult` and records
// neither metric.
TEST_F(ActorToolJavaScriptFeatureUtilTest,
       NonDictResultFailsWithoutRecordingMetrics) {
  base::HistogramTester histogram_tester;
  base::ElapsedTimer timer;
  base::Value non_dict_value("invalid");

  ToolExecutionResult result = ParseJavaScriptResultWithResultCode(
      ActorJavaScriptCall::kClick, &FakeTranslator, &non_dict_value, timer);

  EXPECT_FALSE(result.IsOk());
  EXPECT_FALSE(result.requires_page_stabilization());
  ASSERT_TRUE(result.internal_code().has_value());
  EXPECT_EQ(result.internal_code().value(),
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptResultCodeHistogramName(ActorJavaScriptCall::kClick), 0);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptExecutionDurationHistogramName(ActorJavaScriptCall::kClick),
      0);
}

// Tests that a dictionary missing the `resultCode` field returns
// `InternalToolErrorCode::kJavascriptFeatureGotInvalidResult` and records
// neither metric.
TEST_F(ActorToolJavaScriptFeatureUtilTest,
       MissingResultCodeFailsWithoutRecordingMetrics) {
  base::HistogramTester histogram_tester;
  base::ElapsedTimer timer;
  base::DictValue dict_value;
  dict_value.Set("message", "no result code");
  base::Value value(std::move(dict_value));

  ToolExecutionResult result = ParseJavaScriptResultWithResultCode(
      ActorJavaScriptCall::kClick, &FakeTranslator, &value, timer);

  EXPECT_FALSE(result.IsOk());
  EXPECT_FALSE(result.requires_page_stabilization());
  ASSERT_TRUE(result.internal_code().has_value());
  EXPECT_EQ(result.internal_code().value(),
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptResultCodeHistogramName(ActorJavaScriptCall::kClick), 0);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptExecutionDurationHistogramName(ActorJavaScriptCall::kClick),
      0);
}

// Tests that a dictionary with a non-numeric `resultCode` returns
// `InternalToolErrorCode::kJavascriptFeatureGotInvalidResult` and records
// neither metric.
TEST_F(ActorToolJavaScriptFeatureUtilTest,
       NonNumericResultCodeFailsWithoutRecordingMetrics) {
  base::HistogramTester histogram_tester;
  base::ElapsedTimer timer;
  base::DictValue dict_value;
  dict_value.Set("resultCode", "not-a-number");
  base::Value value(std::move(dict_value));

  ToolExecutionResult result = ParseJavaScriptResultWithResultCode(
      ActorJavaScriptCall::kClick, &FakeTranslator, &value, timer);

  EXPECT_FALSE(result.IsOk());
  EXPECT_FALSE(result.requires_page_stabilization());
  ASSERT_TRUE(result.internal_code().has_value());
  EXPECT_EQ(result.internal_code().value(),
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptResultCodeHistogramName(ActorJavaScriptCall::kClick), 0);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptExecutionDurationHistogramName(ActorJavaScriptCall::kClick),
      0);
}

// Tests that a valid error response records both execution duration and the
// translated result code, and captures the error message when present.
TEST_F(ActorToolJavaScriptFeatureUtilTest,
       ErrorResultRecordsDurationAndResultCode) {
  base::HistogramTester histogram_tester;
  base::ElapsedTimer timer;
  base::DictValue dict_value;
  dict_value.Set("resultCode", 1);
  dict_value.Set("message", "out of bounds");
  base::Value value(std::move(dict_value));

  ToolExecutionResult result = ParseJavaScriptResultWithResultCode(
      ActorJavaScriptCall::kClick, &FakeTranslator, &value, timer);

  EXPECT_FALSE(result.IsOk());
  EXPECT_FALSE(result.requires_page_stabilization());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kCoordinatesOutOfBounds);
  ASSERT_TRUE(result.message().has_value());
  EXPECT_EQ(result.message().value(), "out of bounds");
  histogram_tester.ExpectUniqueSample(
      GetJavaScriptResultCodeHistogramName(ActorJavaScriptCall::kClick),
      mojom::ActionResultCode::kCoordinatesOutOfBounds, 1);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptExecutionDurationHistogramName(ActorJavaScriptCall::kClick),
      1);
}

// Tests that a valid error response without a message field succeeds in parsing
// and records metrics without a message.
TEST_F(ActorToolJavaScriptFeatureUtilTest,
       ErrorResultWithoutMessageRecordsMetrics) {
  base::HistogramTester histogram_tester;
  base::ElapsedTimer timer;
  base::DictValue dict_value;
  dict_value.Set("resultCode", 1);
  base::Value value(std::move(dict_value));

  ToolExecutionResult result = ParseJavaScriptResultWithResultCode(
      ActorJavaScriptCall::kClick, &FakeTranslator, &value, timer);

  EXPECT_FALSE(result.IsOk());
  EXPECT_FALSE(result.requires_page_stabilization());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kCoordinatesOutOfBounds);
  EXPECT_FALSE(result.message().has_value());
  histogram_tester.ExpectUniqueSample(
      GetJavaScriptResultCodeHistogramName(ActorJavaScriptCall::kClick),
      mojom::ActionResultCode::kCoordinatesOutOfBounds, 1);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptExecutionDurationHistogramName(ActorJavaScriptCall::kClick),
      1);
}

// Tests that a successful response records both execution duration and `kOk`,
// and sets `requires_page_stabilization` to true.
TEST_F(ActorToolJavaScriptFeatureUtilTest,
       SuccessResultRecordsDurationAndResultCode) {
  base::HistogramTester histogram_tester;
  base::ElapsedTimer timer;
  base::DictValue dict_value;
  dict_value.Set("resultCode", 0);
  base::Value value(std::move(dict_value));

  ToolExecutionResult result = ParseJavaScriptResultWithResultCode(
      ActorJavaScriptCall::kClick, &FakeTranslator, &value, timer);

  EXPECT_TRUE(result.IsOk());
  EXPECT_TRUE(result.requires_page_stabilization());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kOk);
  EXPECT_FALSE(result.message().has_value());
  histogram_tester.ExpectUniqueSample(
      GetJavaScriptResultCodeHistogramName(ActorJavaScriptCall::kClick),
      mojom::ActionResultCode::kOk, 1);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptExecutionDurationHistogramName(ActorJavaScriptCall::kClick),
      1);
}

// Tests that a successful response with an optional message records metrics,
// sets `requires_page_stabilization` to true, and preserves the message.
TEST_F(ActorToolJavaScriptFeatureUtilTest,
       SuccessResultWithMessageRecordsMetrics) {
  base::HistogramTester histogram_tester;
  base::ElapsedTimer timer;
  base::DictValue dict_value;
  dict_value.Set("resultCode", 0);
  dict_value.Set("message", "action completed successfully");
  base::Value value(std::move(dict_value));

  ToolExecutionResult result = ParseJavaScriptResultWithResultCode(
      ActorJavaScriptCall::kClick, &FakeTranslator, &value, timer);

  EXPECT_TRUE(result.IsOk());
  EXPECT_TRUE(result.requires_page_stabilization());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kOk);
  ASSERT_TRUE(result.message().has_value());
  EXPECT_EQ(result.message().value(), "action completed successfully");
  histogram_tester.ExpectUniqueSample(
      GetJavaScriptResultCodeHistogramName(ActorJavaScriptCall::kClick),
      mojom::ActionResultCode::kOk, 1);
  histogram_tester.ExpectTotalCount(
      GetJavaScriptExecutionDurationHistogramName(ActorJavaScriptCall::kClick),
      1);
}

}  // namespace

}  // namespace actor

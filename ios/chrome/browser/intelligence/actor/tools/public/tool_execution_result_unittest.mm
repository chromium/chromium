// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/actor/public/mojom/actor_types.mojom.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

struct ToolExecutionResultTestCase {
  InternalToolErrorCode internal_code;
  mojom::ActionResultCode expected_result_code;
  std::string expected_message;
};

class ToolExecutionResultTest
    : public PlatformTest,
      public testing::WithParamInterface<ToolExecutionResultTestCase> {};

TEST_P(ToolExecutionResultTest, ConstructorMapsErrorCodeToResultCode) {
  const ToolExecutionResultTestCase& test_case = GetParam();
  ToolExecutionResult result(test_case.internal_code);
  EXPECT_EQ(test_case.expected_result_code, result.code())
      << "Failed for internal_code: "
      << static_cast<int>(test_case.internal_code);
}

TEST_P(ToolExecutionResultTest, GetToolExecutionResultMessage) {
  const ToolExecutionResultTestCase& test_case = GetParam();
  ToolExecutionResult result(test_case.internal_code);
  EXPECT_EQ(test_case.expected_message, GetToolExecutionResultMessage(result))
      << "Failed for internal_code: "
      << static_cast<int>(test_case.internal_code);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ToolExecutionResultTest,
    testing::Values(
        ToolExecutionResultTestCase{
            InternalToolErrorCode::kUnsupportedAction,
            mojom::ActionResultCode::kToolUnknown,
            "There isn't a tool to support this action."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::kToolDisabledByFeature,
            mojom::ActionResultCode::kToolUnknown,
            "Tool is disabled via feature parameter."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::kExecutionMissingDependencies,
            mojom::ActionResultCode::kArgumentsInvalid,
            "On tool execution, required dependencies were missing."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::kCreationMissingRequiredFields,
            mojom::ActionResultCode::kArgumentsInvalid,
            "On tool creation, required fields were missing."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::kCreationTargetTabNotFound,
            mojom::ActionResultCode::kTabWentAway,
            "On tool creation, target tab isn't in any Browser."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::kCreationMissingWebStateList,
            mojom::ActionResultCode::kTabWentAway,
            "On tool creation, failed to get WebStateList."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::kCreationMissingWebState,
            mojom::ActionResultCode::kTabWentAway,
            "On tool creation, failed to get WebState."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult,
            mojom::ActionResultCode::kArgumentsInvalid,
            "The JavaScriptFeature got an unexpected response from the "
            "JavaScript function."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::
                kJavascriptFeatureFailedToCallJavaScriptFunction,
            mojom::ActionResultCode::kArgumentsInvalid,
            "The JavaScriptFeature::CallJavaScriptFunction call failed."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::
                kJavascriptFeatureFailedInJavaScriptExecution,
            mojom::ActionResultCode::kArgumentsInvalid,
            "The JavaScriptFeature failed when executing the JavaScript."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::kActorTargetWebFrameInvalidated,
            mojom::ActionResultCode::kFrameWentAway,
            "The target WebFrame was invalidated before the JavaScript "
            "function could be called."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::kNavigationInvalidURL,
            mojom::ActionResultCode::kNavigateInvalidUrl,
            "Navigation failed due to invalid destination URL."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::kNavigationTabNotRealized,
            mojom::ActionResultCode::kNavigateFailedToStart,
            "Navigation failed because the target tab is not realized."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::kHistoryBackNotPossible,
            mojom::ActionResultCode::kHistoryNoBackEntries, "Cannot go back."},
        ToolExecutionResultTestCase{
            InternalToolErrorCode::kHistoryForwardNotPossible,
            mojom::ActionResultCode::kHistoryNoForwardEntries,
            "Cannot go forward."}),
    [](const testing::TestParamInfo<ToolExecutionResultTest::ParamType>& info) {
      switch (info.param.internal_code) {
        case InternalToolErrorCode::kUnsupportedAction:
          return "kUnsupportedAction";
        case InternalToolErrorCode::kToolDisabledByFeature:
          return "kToolDisabledByFeature";
        case InternalToolErrorCode::kExecutionMissingDependencies:
          return "kExecutionMissingDependencies";
        case InternalToolErrorCode::kCreationMissingRequiredFields:
          return "kCreationMissingRequiredFields";
        case InternalToolErrorCode::kCreationTargetTabNotFound:
          return "kCreationTargetTabNotFound";
        case InternalToolErrorCode::kCreationMissingWebStateList:
          return "kCreationMissingWebStateList";
        case InternalToolErrorCode::kCreationMissingWebState:
          return "kCreationMissingWebState";
        case InternalToolErrorCode::kJavascriptFeatureGotInvalidResult:
          return "kJavascriptFeatureGotInvalidResult";
        case InternalToolErrorCode::
            kJavascriptFeatureFailedToCallJavaScriptFunction:
          return "kJavascriptFeatureFailedToCallJavaScriptFunction";
        case InternalToolErrorCode::
            kJavascriptFeatureFailedInJavaScriptExecution:
          return "kJavascriptFeatureFailedInJavaScriptExecution";
        case InternalToolErrorCode::kActorTargetWebFrameInvalidated:
          return "kActorTargetWebFrameInvalidated";
        case InternalToolErrorCode::kNavigationInvalidURL:
          return "kNavigationInvalidURL";
        case InternalToolErrorCode::kNavigationTabNotRealized:
          return "kNavigationTabNotRealized";
        case InternalToolErrorCode::kHistoryBackNotPossible:
          return "kHistoryBackNotPossible";
        case InternalToolErrorCode::kHistoryForwardNotPossible:
          return "kHistoryForwardNotPossible";
      }
    });

TEST_F(ToolExecutionResultTest,
       GetToolExecutionResultMessage_ExternalCodeOnly) {
  ToolExecutionResult result(mojom::ActionResultCode::kOk);
  EXPECT_EQ(GetToolExecutionResultMessage(result),
            "Tool resulted in mojom::ActionResultCode[0]");
}

}  // namespace actor

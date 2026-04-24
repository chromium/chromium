// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"

#import "components/actor/public/mojom/actor_types.mojom.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

struct ActorToolErrorTestCase {
  ActorToolErrorCode internal_code;
  mojom::ActionResultCode expected_external_code;
  std::string expected_message;
};

class ActorToolErrorTest
    : public PlatformTest,
      public testing::WithParamInterface<ActorToolErrorTestCase> {};

TEST_P(ActorToolErrorTest, ConstructorMapsInternalToExternalCode) {
  const ActorToolErrorTestCase& test_case = GetParam();
  ActorToolError error(test_case.internal_code);
  EXPECT_EQ(test_case.expected_external_code, error.external_code)
      << "Failed for internal_code: "
      << static_cast<int>(test_case.internal_code);
}

TEST_P(ActorToolErrorTest, GetActorToolErrorMessage) {
  const ActorToolErrorTestCase& test_case = GetParam();
  ActorToolError error(test_case.internal_code);
  EXPECT_EQ(test_case.expected_message, GetActorToolErrorMessage(error))
      << "Failed for internal_code: "
      << static_cast<int>(test_case.internal_code);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ActorToolErrorTest,
    testing::Values(
        ActorToolErrorTestCase{ActorToolErrorCode::kUnsupportedAction,
                               mojom::ActionResultCode::kToolUnknown,
                               "There isn't a tool to support this action."},
        ActorToolErrorTestCase{ActorToolErrorCode::kToolDisabledByFeature,
                               mojom::ActionResultCode::kToolUnknown,
                               "Tool is disabled via feature parameter."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kExecutionMissingDependencies,
            mojom::ActionResultCode::kArgumentsInvalid,
            "On tool execution, required dependencies were missing."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kCreationMissingRequiredFields,
            mojom::ActionResultCode::kArgumentsInvalid,
            "On tool creation, required fields were missing."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kCreationTargetTabNotFound,
            mojom::ActionResultCode::kTabWentAway,
            "On tool creation, target tab isn't in any Browser."},
        ActorToolErrorTestCase{ActorToolErrorCode::kCreationMissingWebStateList,
                               mojom::ActionResultCode::kTabWentAway,
                               "On tool creation, failed to get WebStateList."},
        ActorToolErrorTestCase{ActorToolErrorCode::kCreationMissingWebState,
                               mojom::ActionResultCode::kTabWentAway,
                               "On tool creation, failed to get WebState."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kJavascriptFeatureGotInvalidResult,
            mojom::ActionResultCode::kArgumentsInvalid,
            "The JavaScriptFeature got an unexpected response from the "
            "JavaScript function."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::
                kJavascriptFeatureFailedToCallJavaScriptFunction,
            mojom::ActionResultCode::kArgumentsInvalid,
            "The JavaScriptFeature::CallJavaScriptFunction call failed."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution,
            mojom::ActionResultCode::kArgumentsInvalid,
            "The JavaScriptFeature failed when executing the JavaScript."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kActorTargetWebStateDestroyed,
            mojom::ActionResultCode::kTabWentAway,
            "The WebState was destroyed while looking for actor target."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kActorTargetInvalidRemoteFrameToken,
            mojom::ActionResultCode::kArgumentsInvalid,
            "Failed to deserialize remote frame token."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kActorTargetFrameNotRegistered,
            mojom::ActionResultCode::kArgumentsInvalid,
            "The target frame was not registered with the "
            "ChildFrameRegistrar."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kActorTargetFrameNotFoundById,
            mojom::ActionResultCode::kArgumentsInvalid,
            "Could not find target frame in WebFramesManager by the frame ID."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kActorTargetMaxDepthExceeded,
            mojom::ActionResultCode::kToolTimeout,
            "The ActorToolTargetJavaScriptFeature reached the maximum "
            "recursion depth."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kActorTargetWebFrameInvalidated,
            mojom::ActionResultCode::kFrameWentAway,
            "The target WebFrame was invalidated before the JavaScript "
            "function could be called."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kNavigationInvalidURL,
            mojom::ActionResultCode::kNavigateInvalidUrl,
            "Navigation failed due to invalid destination URL."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kNavigationTabNotRealized,
            mojom::ActionResultCode::kNavigateFailedToStart,
            "Navigation failed because the target tab is not realized."},
        ActorToolErrorTestCase{ActorToolErrorCode::kHistoryBackNotPossible,
                               mojom::ActionResultCode::kHistoryNoBackEntries,
                               "Cannot go back."},
        ActorToolErrorTestCase{
            ActorToolErrorCode::kHistoryForwardNotPossible,
            mojom::ActionResultCode::kHistoryNoForwardEntries,
            "Cannot go forward."}),
    [](const testing::TestParamInfo<ActorToolErrorTest::ParamType>& info) {
      switch (info.param.internal_code) {
        case ActorToolErrorCode::kUnsupportedAction:
          return "kUnsupportedAction";
        case ActorToolErrorCode::kToolDisabledByFeature:
          return "kToolDisabledByFeature";
        case ActorToolErrorCode::kExecutionMissingDependencies:
          return "kExecutionMissingDependencies";
        case ActorToolErrorCode::kCreationMissingRequiredFields:
          return "kCreationMissingRequiredFields";
        case ActorToolErrorCode::kCreationTargetTabNotFound:
          return "kCreationTargetTabNotFound";
        case ActorToolErrorCode::kCreationMissingWebStateList:
          return "kCreationMissingWebStateList";
        case ActorToolErrorCode::kCreationMissingWebState:
          return "kCreationMissingWebState";
        case ActorToolErrorCode::kJavascriptFeatureGotInvalidResult:
          return "kJavascriptFeatureGotInvalidResult";
        case ActorToolErrorCode::
            kJavascriptFeatureFailedToCallJavaScriptFunction:
          return "kJavascriptFeatureFailedToCallJavaScriptFunction";
        case ActorToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution:
          return "kJavascriptFeatureFailedInJavaScriptExecution";
        case ActorToolErrorCode::kActorTargetWebStateDestroyed:
          return "kActorTargetWebStateDestroyed";
        case ActorToolErrorCode::kActorTargetInvalidRemoteFrameToken:
          return "kActorTargetInvalidRemoteFrameToken";
        case ActorToolErrorCode::kActorTargetFrameNotRegistered:
          return "kActorTargetFrameNotRegistered";
        case ActorToolErrorCode::kActorTargetFrameNotFoundById:
          return "kActorTargetFrameNotFoundById";
        case ActorToolErrorCode::kActorTargetMaxDepthExceeded:
          return "kActorTargetMaxDepthExceeded";
        case ActorToolErrorCode::kActorTargetWebFrameInvalidated:
          return "kActorTargetWebFrameInvalidated";
        case ActorToolErrorCode::kNavigationInvalidURL:
          return "kNavigationInvalidURL";
        case ActorToolErrorCode::kNavigationTabNotRealized:
          return "kNavigationTabNotRealized";
        case ActorToolErrorCode::kHistoryBackNotPossible:
          return "kHistoryBackNotPossible";
        case ActorToolErrorCode::kHistoryForwardNotPossible:
          return "kHistoryForwardNotPossible";
      }
    });

}  // namespace actor

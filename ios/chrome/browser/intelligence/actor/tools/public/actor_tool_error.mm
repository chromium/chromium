// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"

#import "base/notreached.h"
#import "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

ActorToolError::ActorToolError(mojom::ActionResultCode external_code,
                               ActorToolErrorCode internal_code,
                               std::optional<std::string> message)
    : external_code(external_code),
      code(internal_code),
      message(std::move(message)) {}

ActorToolError::ActorToolError(mojom::ActionResultCode external_code,
                               std::optional<std::string> message)
    : ActorToolError(external_code,
                     // TODO(crbug.com/505037793): Stop setting this once the
                     // field is made optional.
                     ActorToolErrorCode::kUnsupportedAction,
                     std::move(message)) {}

ActorToolError::ActorToolError(ActorToolErrorCode internal_code,
                               std::optional<std::string> message)
    : code(internal_code), message(std::move(message)) {
  switch (internal_code) {
    case ActorToolErrorCode::kUnsupportedAction:
    case ActorToolErrorCode::kToolDisabledByFeature:
      external_code = mojom::ActionResultCode::kToolUnknown;
      break;
    case ActorToolErrorCode::kExecutionMissingDependencies:
    case ActorToolErrorCode::kCreationMissingRequiredFields:
      external_code = mojom::ActionResultCode::kArgumentsInvalid;
      break;
    case ActorToolErrorCode::kCreationTargetTabNotFound:
    case ActorToolErrorCode::kCreationMissingWebStateList:
    case ActorToolErrorCode::kCreationMissingWebState:
      external_code = mojom::ActionResultCode::kTabWentAway;
      break;
    case ActorToolErrorCode::kJavascriptFeatureGotInvalidResult:
    case ActorToolErrorCode::kJavascriptFeatureFailedToCallJavaScriptFunction:
    case ActorToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution:
      // TODO(crbug.com/505037793): Add a more appropriate ActionResultCode for
      // this case, or Bling-specific errors more generally.
      external_code = mojom::ActionResultCode::kArgumentsInvalid;
      break;
    case ActorToolErrorCode::kActorTargetWebStateDestroyed:
      external_code = mojom::ActionResultCode::kTabWentAway;
      break;
    case ActorToolErrorCode::kActorTargetInvalidRemoteFrameToken:
    case ActorToolErrorCode::kActorTargetFrameNotRegistered:
    case ActorToolErrorCode::kActorTargetFrameNotFoundById:
      external_code = mojom::ActionResultCode::kArgumentsInvalid;
      break;
    case ActorToolErrorCode::kActorTargetMaxDepthExceeded:
      external_code = mojom::ActionResultCode::kToolTimeout;
      break;
    case ActorToolErrorCode::kActorTargetWebFrameInvalidated:
      external_code = mojom::ActionResultCode::kFrameWentAway;
      break;
    case ActorToolErrorCode::kNavigationInvalidURL:
      external_code = mojom::ActionResultCode::kNavigateInvalidUrl;
      break;
    case ActorToolErrorCode::kNavigationTabNotRealized:
      external_code = mojom::ActionResultCode::kNavigateFailedToStart;
      break;
    case ActorToolErrorCode::kHistoryBackNotPossible:
      external_code = mojom::ActionResultCode::kHistoryNoBackEntries;
      break;
    case ActorToolErrorCode::kHistoryForwardNotPossible:
      external_code = mojom::ActionResultCode::kHistoryNoForwardEntries;
      break;
    default:
      NOTREACHED();
  }
}

ActorToolError::~ActorToolError() = default;

ActorToolError::ActorToolError(const ActorToolError& other) = default;
ActorToolError& ActorToolError::operator=(const ActorToolError& other) =
    default;

ActorToolError::ActorToolError(ActorToolError&& other) = default;
ActorToolError& ActorToolError::operator=(ActorToolError&& other) = default;

std::string GetActorToolErrorMessage(const ActorToolError& error) {
  if (error.message.has_value()) {
    return error.message.value();
  }
  switch (error.code) {
    case ActorToolErrorCode::kUnsupportedAction:
      return "There isn't a tool to support this action.";
    case ActorToolErrorCode::kToolDisabledByFeature:
      return "Tool is disabled via feature parameter.";
    case ActorToolErrorCode::kExecutionMissingDependencies:
      return "On tool execution, required dependencies were missing.";
    case ActorToolErrorCode::kCreationMissingRequiredFields:
      return "On tool creation, required fields were missing.";
    case ActorToolErrorCode::kCreationTargetTabNotFound:
      return "On tool creation, target tab isn't in any Browser.";
    case ActorToolErrorCode::kCreationMissingWebStateList:
      return "On tool creation, failed to get WebStateList.";
    case ActorToolErrorCode::kCreationMissingWebState:
      return "On tool creation, failed to get WebState.";
    case ActorToolErrorCode::kJavascriptFeatureGotInvalidResult:
      return "The JavaScriptFeature got an unexpected response from the "
             "JavaScript function.";
    case ActorToolErrorCode::kJavascriptFeatureFailedToCallJavaScriptFunction:
      return "The JavaScriptFeature::CallJavaScriptFunction call failed.";
    case ActorToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution:
      return "The JavaScriptFeature failed when executing the JavaScript.";
    case ActorToolErrorCode::kActorTargetWebStateDestroyed:
      return "The WebState was destroyed while looking for actor target.";
    case ActorToolErrorCode::kActorTargetInvalidRemoteFrameToken:
      return "Failed to deserialize remote frame token.";
    case ActorToolErrorCode::kActorTargetFrameNotRegistered:
      return "The target frame was not registered with the "
             "ChildFrameRegistrar.";
    case ActorToolErrorCode::kActorTargetFrameNotFoundById:
      return "Could not find target frame in WebFramesManager by the frame ID.";
    case ActorToolErrorCode::kActorTargetMaxDepthExceeded:
      return "The ActorToolTargetJavaScriptFeature reached the maximum "
             "recursion depth.";
    case ActorToolErrorCode::kActorTargetWebFrameInvalidated:
      return "The target WebFrame was invalidated before the JavaScript "
             "function could be called.";
    case ActorToolErrorCode::kNavigationInvalidURL:
      return "Navigation failed due to invalid destination URL.";
    case ActorToolErrorCode::kNavigationTabNotRealized:
      return "Navigation failed because the target tab is not realized.";
    case ActorToolErrorCode::kHistoryBackNotPossible:
      return "Cannot go back.";
    case ActorToolErrorCode::kHistoryForwardNotPossible:
      return "Cannot go forward.";
  }
  NOTREACHED();
}

}  // namespace actor

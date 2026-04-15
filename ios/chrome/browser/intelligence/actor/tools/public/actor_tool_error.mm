// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"

#import "base/notreached.h"

namespace actor {

ActorToolError::ActorToolError(ActorToolErrorCode code,
                               std::optional<std::string> message)
    : code(code), message(std::move(message)) {}

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
    case ActorToolErrorCode::kUnknown:
      return "Unknown error.";
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

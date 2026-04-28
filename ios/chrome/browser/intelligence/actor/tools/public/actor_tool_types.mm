// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

#import "base/notreached.h"
#import "base/strings/stringprintf.h"
#import "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

ToolExecutionResult::ToolExecutionResult(mojom::ActionResultCode external_code,
                                         InternalToolErrorCode internal_code,
                                         std::optional<std::string> message)
    : external_code_(external_code),
      internal_code_(internal_code),
      message_(std::move(message)) {}

ToolExecutionResult::ToolExecutionResult(mojom::ActionResultCode external_code,
                                         std::optional<std::string> message)
    : external_code_(external_code),
      internal_code_(std::nullopt),
      message_(std::move(message)) {}

ToolExecutionResult::ToolExecutionResult(InternalToolErrorCode internal_code,
                                         std::optional<std::string> message)
    : internal_code_(internal_code), message_(std::move(message)) {
  mojom::ActionResultCode external_code =
      mojom::ActionResultCode::kArgumentsInvalid;
  switch (internal_code) {
    case InternalToolErrorCode::kUnsupportedAction:
    case InternalToolErrorCode::kToolDisabledByFeature:
      external_code = mojom::ActionResultCode::kToolUnknown;
      break;
    case InternalToolErrorCode::kExecutionMissingDependencies:
    case InternalToolErrorCode::kCreationMissingRequiredFields:
      external_code = mojom::ActionResultCode::kArgumentsInvalid;
      break;
    case InternalToolErrorCode::kCreationTargetTabNotFound:
    case InternalToolErrorCode::kCreationMissingWebStateList:
    case InternalToolErrorCode::kCreationMissingWebState:
      external_code = mojom::ActionResultCode::kTabWentAway;
      break;
    case InternalToolErrorCode::kJavascriptFeatureGotInvalidResult:
    case InternalToolErrorCode::
        kJavascriptFeatureFailedToCallJavaScriptFunction:
    case InternalToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution:
      // TODO(crbug.com/505037793): Add a more appropriate ActionResultCode for
      // this case, or Bling-specific errors more generally.
      external_code = mojom::ActionResultCode::kArgumentsInvalid;
      break;
    case InternalToolErrorCode::kActorTargetWebStateDestroyed:
      external_code = mojom::ActionResultCode::kTabWentAway;
      break;
    case InternalToolErrorCode::kActorTargetInvalidRemoteFrameToken:
    case InternalToolErrorCode::kActorTargetFrameNotRegistered:
    case InternalToolErrorCode::kActorTargetFrameNotFoundById:
      external_code = mojom::ActionResultCode::kArgumentsInvalid;
      break;
    case InternalToolErrorCode::kActorTargetMaxDepthExceeded:
      external_code = mojom::ActionResultCode::kToolTimeout;
      break;
    case InternalToolErrorCode::kActorTargetWebFrameInvalidated:
      external_code = mojom::ActionResultCode::kFrameWentAway;
      break;
    case InternalToolErrorCode::kNavigationInvalidURL:
      external_code = mojom::ActionResultCode::kNavigateInvalidUrl;
      break;
    case InternalToolErrorCode::kNavigationTabNotRealized:
      external_code = mojom::ActionResultCode::kNavigateFailedToStart;
      break;
    case InternalToolErrorCode::kHistoryBackNotPossible:
      external_code = mojom::ActionResultCode::kHistoryNoBackEntries;
      break;
    case InternalToolErrorCode::kHistoryForwardNotPossible:
      external_code = mojom::ActionResultCode::kHistoryNoForwardEntries;
      break;
  }
  external_code_ = external_code;
}

ToolExecutionResult::~ToolExecutionResult() = default;

ToolExecutionResult::ToolExecutionResult(const ToolExecutionResult& other) =
    default;
ToolExecutionResult& ToolExecutionResult::operator=(
    const ToolExecutionResult& other) = default;

ToolExecutionResult::ToolExecutionResult(ToolExecutionResult&& other) = default;
ToolExecutionResult& ToolExecutionResult::operator=(
    ToolExecutionResult&& other) = default;

std::string GetToolExecutionResultMessage(const ToolExecutionResult& result) {
  if (result.message().has_value()) {
    return result.message().value();
  }
  if (result.internal_code()) {
    switch (result.internal_code().value()) {
      case InternalToolErrorCode::kUnsupportedAction:
        return "There isn't a tool to support this action.";
      case InternalToolErrorCode::kToolDisabledByFeature:
        return "Tool is disabled via feature parameter.";
      case InternalToolErrorCode::kExecutionMissingDependencies:
        return "On tool execution, required dependencies were missing.";
      case InternalToolErrorCode::kCreationMissingRequiredFields:
        return "On tool creation, required fields were missing.";
      case InternalToolErrorCode::kCreationTargetTabNotFound:
        return "On tool creation, target tab isn't in any Browser.";
      case InternalToolErrorCode::kCreationMissingWebStateList:
        return "On tool creation, failed to get WebStateList.";
      case InternalToolErrorCode::kCreationMissingWebState:
        return "On tool creation, failed to get WebState.";
      case InternalToolErrorCode::kJavascriptFeatureGotInvalidResult:
        return "The JavaScriptFeature got an unexpected response from the "
               "JavaScript function.";
      case InternalToolErrorCode::
          kJavascriptFeatureFailedToCallJavaScriptFunction:
        return "The JavaScriptFeature::CallJavaScriptFunction call failed.";
      case InternalToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution:
        return "The JavaScriptFeature failed when executing the JavaScript.";
      case InternalToolErrorCode::kActorTargetWebStateDestroyed:
        return "The WebState was destroyed while looking for actor target.";
      case InternalToolErrorCode::kActorTargetInvalidRemoteFrameToken:
        return "Failed to deserialize remote frame token.";
      case InternalToolErrorCode::kActorTargetFrameNotRegistered:
        return "The target frame was not registered with the "
               "ChildFrameRegistrar.";
      case InternalToolErrorCode::kActorTargetFrameNotFoundById:
        return "Could not find target frame in WebFramesManager by the frame "
               "ID.";
      case InternalToolErrorCode::kActorTargetMaxDepthExceeded:
        return "The ActorToolTargetJavaScriptFeature reached the maximum "
               "recursion depth.";
      case InternalToolErrorCode::kActorTargetWebFrameInvalidated:
        return "The target WebFrame was invalidated before the JavaScript "
               "function could be called.";
      case InternalToolErrorCode::kNavigationInvalidURL:
        return "Navigation failed due to invalid destination URL.";
      case InternalToolErrorCode::kNavigationTabNotRealized:
        return "Navigation failed because the target tab is not realized.";
      case InternalToolErrorCode::kHistoryBackNotPossible:
        return "Cannot go back.";
      case InternalToolErrorCode::kHistoryForwardNotPossible:
        return "Cannot go forward.";
    }
    // This case is only reached if an InternalToolErrorCode is added but not
    // handled here.
    NOTREACHED();
  }
  return base::StringPrintf("Tool resulted in mojom::ActionResultCode[%d]",
                            result.code());
}

}  // namespace actor

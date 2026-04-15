// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_ERROR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_ERROR_H_

#import <optional>
#import <string>

namespace actor {

// Error codes for ActorTool in Chrome on iOS.
//
// This enum follows the structure of `ActionResultCode` from
// chrome/common/actor.mojom, using numbered ranges to group errors by category.
//
// 0-99: General errors that aren't specific to any tool.
// 100-199: Tool creation errors that aren't specific to any tool.
// 200-299: JavaScriptFeature errors that aren't specific to any tool.
// X00-X99 - A tool-specific error; each tool gets a reserved range of values.
enum class ActorToolErrorCode {
  // General errors (0-99).

  // Default error code for unknown or unspecified failures.
  // A more specific error code should be used when possible.
  kUnknown = 0,
  // There is not an ActorTool that supports the given action.
  kUnsupportedAction = 1,
  // The specific tool type is disabled via the 'DisabledTools' feature
  // parameter.
  kToolDisabledByFeature = 2,
  // The tool failed to execute because required dependencies were missing.
  // Tool-specific execution errors should be added in their respective
  // code ranges.
  kExecutionMissingDependencies = 3,

  // Tool Creation Errors (100-199).

  // The tool creation failed because the action proto was missing required
  // fields.
  kCreationMissingRequiredFields = 100,
  // The target tab for the action could not be found.
  kCreationTargetTabNotFound = 101,
  // Failed to retrieve the WebStateList from the browser.
  kCreationMissingWebStateList = 102,
  // Failed to retrieve the WebState from the WebStateList.
  kCreationMissingWebState = 103,

  // General JavaScriptFeature Errors (200-299).

  // The JavaScript feature received a result from the JavaScript that didn't
  // match the format it expected.
  kJavascriptFeatureGotInvalidResult = 200,
  // The JavaScriptFeature::CallJavaScriptFunction call failed.
  kJavascriptFeatureFailedToCallJavaScriptFunction = 201,
  // The JavaScript function failed during execution.
  // When this is used, the ActorToolError.message should be populated with an
  // error message from the JavaScript that provides more context.
  kJavascriptFeatureFailedInJavaScriptExecution = 202,
  // The ActorToolTargetJavaScriptFeature failed because the WebState was
  // destroyed while trying to find the target frame.
  kActorTargetWebStateDestroyed = 203,
  // The ActorToolTargetJavaScriptFeature received a remote frame token that
  // could not be deserialized.
  kActorTargetInvalidRemoteFrameToken = 204,
  // The target frame was not registered with the ChildFrameRegistrar.
  kActorTargetFrameNotRegistered = 205,
  // The target frame could not be found in the WebFramesManager after
  // registration.
  kActorTargetFrameNotFoundById = 206,
  // The ActorToolTargetJavaScriptFeature reached the maximum recursion depth.
  kActorTargetMaxDepthExceeded = 207,
  // The target WebFrame was invalidated before the JavaScript function could
  // be called.
  kActorTargetWebFrameInvalidated = 208,

  // Navigation Tool Errors (300-399).

  // The requested URL for navigation was invalid.
  kNavigationInvalidURL = 300,
  // The tab to be navigated is not realized.
  // See docs/ios/unrealized_web_state.md.
  kNavigationTabNotRealized = 301,

  // History Tool Errors (400-499).

  // It is not possible to navigate back in the tab's history.
  kHistoryBackNotPossible = 400,
  // It is not possible to navigate forward in the tab's history.
  kHistoryForwardNotPossible = 401,
};

// Represents an ActorTool error.
struct ActorToolError {
  ActorToolError(ActorToolErrorCode code,
                 std::optional<std::string> message = std::nullopt);
  ~ActorToolError();

  ActorToolError(const ActorToolError& other);
  ActorToolError& operator=(const ActorToolError& other);

  ActorToolError(ActorToolError&& other);
  ActorToolError& operator=(ActorToolError&& other);

  ActorToolErrorCode code;
  // If not set, a default message for the error code will be used.
  // This is primarily used for kJavascriptExecutionFailed to provide
  // detailed JS error info.
  std::optional<std::string> message = std::nullopt;
};

// Returns a localized error message for the given error.
std::string GetActorToolErrorMessage(const ActorToolError& error);

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_ERROR_H_

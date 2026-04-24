// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_ERROR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_ERROR_H_

#import <optional>
#import <string>

#import "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

// TODO(crbug.com/505037793): Make this enum be scoped to errors that are only
// relevant to Chrome on iOS. We should rename it and make it an implementation
// detail of the ActorToolResult.
enum class ActorToolErrorCode {
  // General errors (0-99).
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
  // TODO(crbug.com/505037793): Make each TS script file use a real error code
  // by
  // making them coordinate failure modes with their JavaScriptFeature class.
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

// Represents the result of executing an ActorTool. This struct contains two
// error codes: an `external_code` which corresponds to the broad,
// platform-agnostic ActionResultCode defined in the mojom API, and an
// `internal_code` which is an iOS-specific ActorToolErrorCode providing more
// detailed information about the failure.
// TODO(crbug.com/505037793): Rename this to ToolResult.
struct ActorToolError {
  ActorToolError(mojom::ActionResultCode external_code,
                 ActorToolErrorCode internal_code,
                 std::optional<std::string> message = std::nullopt);
  ActorToolError(actor::mojom::ActionResultCode external_code,
                 std::optional<std::string> message = std::nullopt);
  // Constructs an error from an internal_code, inferring the corresponding
  // external_code automatically.
  // TODO(crbug.com/505037793): Stop using this in favor of other constructors
  // after migration.
  ActorToolError(ActorToolErrorCode internal_code,
                 std::optional<std::string> message = std::nullopt);
  ~ActorToolError();

  ActorToolError(const ActorToolError& other);
  ActorToolError& operator=(const ActorToolError& other);

  ActorToolError(ActorToolError&& other);
  ActorToolError& operator=(ActorToolError&& other);

  // The `external_code` is what is surfaced to callers and matches the codes
  // used on Desktop. The internal `code` is used when there is not an external
  // code that is specific enough for our use case.
  mojom::ActionResultCode external_code;
  // TODO(crbug.com/505037793): Rename to internal_code and make it optional.
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

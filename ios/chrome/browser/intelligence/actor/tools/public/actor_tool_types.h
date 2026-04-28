// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_TYPES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_TYPES_H_

#import <optional>
#import <string>

#import "base/functional/callback_forward.h"
#import "base/types/expected.h"
#import "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

// iOS-specific error codes for tool execution failures.
enum class InternalToolErrorCode {
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
  // The JavaScript function failed during execution. When this is used, the
  // ToolExecutionResult.message should be populated with an error message from
  // the JavaScript that provides more context.
  // TODO(crbug.com/505037793): Make each TS script file use a real error code
  // by making them coordinate failure modes with their JavaScriptFeature class.
  kJavascriptFeatureFailedInJavaScriptExecution = 202,
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

// Wraps the result of a tool execution.
//
// This is based on the ActionResult used on Desktop. See
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/common/actor.mojom;l=647;drc=baa6da4a73b262b329328494bdbcaf088374b745
//
// This struct contains two error codes:
//  - `external_code` which corresponds to the broad, platform-agnostic
//                    ActionResultCode defined in mojom.
//  - `internal_code` which is an iOS-specific InternalToolErrorCode providing
//    more detailed information about the failure.
struct ToolExecutionResult {
  ToolExecutionResult(mojom::ActionResultCode external_code,
                      std::optional<std::string> message = std::nullopt);
  ToolExecutionResult(mojom::ActionResultCode external_code,
                      InternalToolErrorCode internal_code,
                      std::optional<std::string> message = std::nullopt);
  // Constructs an error from an internal_code, inferring the corresponding
  // external_code automatically.
  // TODO(crbug.com/505037793): Stop using this in favor of other constructors
  // after migration.
  ToolExecutionResult(InternalToolErrorCode internal_code,
                      std::optional<std::string> message = std::nullopt);
  ~ToolExecutionResult();

  ToolExecutionResult(const ToolExecutionResult& other);
  ToolExecutionResult& operator=(const ToolExecutionResult& other);

  ToolExecutionResult(ToolExecutionResult&& other);
  ToolExecutionResult& operator=(ToolExecutionResult&& other);

  mojom::ActionResultCode code() const { return external_code_; }
  std::optional<std::string> message() const { return message_; }
  bool IsOk() const { return external_code_ == mojom::ActionResultCode::kOk; }
  static ToolExecutionResult Ok() {
    return ToolExecutionResult(mojom::ActionResultCode::kOk);
  }
  // Temporary helpers while we migrate callsites away from expecting a
  // base::expected.
  // TODO(crbug.com/505037793): remove these helpers once callsites use the
  // above accessors instead.
  std::optional<InternalToolErrorCode> internal_code() const {
    return internal_code_;
  }

 private:
  // The `external_code` is what is surfaced to callers and matches the codes
  // used on Desktop. The `internal_code` is used when there is not an external
  // code that is specific enough for actuation in Chrome on iOS.
  mojom::ActionResultCode external_code_;
  std::optional<InternalToolErrorCode> internal_code_;
  // If unset, a default message for the error codes will be used.
  // This is primarily used for kJavascriptFeatureFailedInJavaScriptExecution
  // to provide detailed JS error info.
  std::optional<std::string> message_ = std::nullopt;
};

// The callback passed to a tool execution, used to report the result of the
// tool execution.
using ToolExecutionCallback = base::OnceCallback<void(ToolExecutionResult)>;

// Returns a message explaining the ToolExecutionResult.
std::string GetToolExecutionResultMessage(const ToolExecutionResult& result);

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_ACTOR_TOOL_TYPES_H_

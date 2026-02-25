// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_ERROR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_ERROR_H_

#import <optional>
#import <string>

// Error codes for actuation in Chrome on iOS.
//
// This enum follows the structure of `ActionResultCode` from
// chrome/common/actor.mojom, using numbered ranges to group errors by category.
//
// 0-99: General errors that aren't specific to any tool.
// 100-199: Tool creation errors that aren't specific to any tool.
// 200-299: JavaScriptFeature errors that aren't specific to any tool.
// X00-X99 - A tool-specific error; each tool gets a reserved range of values.
enum class ActuationErrorCode {
  // General errors (0-99).

  // Default error code for unknown or unspecified failures.
  // A more specific error code should be used when possible.
  kUnknown = 0,
  // There is not an ActuationTool that supports the given action.
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

  // JavaScript Feature Errors (200-299).

  // The JavaScript feature received a result from the JavaScript that didn't
  // match the format it expected.
  kJavascriptFeatureGotInvalidResult = 200,
  // The JavaScriptFeature::CallJavaScriptFunction call failed.
  kJavascriptFeatureFailedToCallJavaScriptFunction = 201,
  // The JavaScript function failed during execution.
  // When this is used, the ActuationError.message should be populated with an
  // error message from the JavaScript that provides more context.
  kJavascriptFeatureFailedInJavaScriptExecution = 202,

  // Navigation Tool Errors (300-399).

  // The requested URL for navigation was invalid.
  kNavigationInvalidURL = 300,
  // The tab to be navigated is not realized.
  // See docs/ios/unrealized_web_state.md.
  kNavigationTabNotRealized = 301,
};

// Represents an error that occurred during actuation.
struct ActuationError {
  ActuationError(ActuationErrorCode code,
                 std::optional<std::string> message = std::nullopt);
  ~ActuationError();

  ActuationError(const ActuationError& other);
  ActuationError& operator=(const ActuationError& other);

  ActuationError(ActuationError&& other);
  ActuationError& operator=(ActuationError&& other);

  ActuationErrorCode code;
  // If not set, a default message for the error code will be used.
  // This is primarily used for kJavascriptExecutionFailed to provide
  // detailed JS error info.
  std::optional<std::string> message = std::nullopt;
};

// Returns a localized error message for the given error.
std::string GetActuationErrorMessage(const ActuationError& error);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_ERROR_H_

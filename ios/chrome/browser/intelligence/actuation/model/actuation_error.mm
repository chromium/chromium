// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"

#import "base/notreached.h"

ActuationError::ActuationError(ActuationErrorCode code,
                               std::optional<std::string> message)
    : code(code), message(std::move(message)) {}

ActuationError::~ActuationError() = default;

ActuationError::ActuationError(const ActuationError& other) = default;
ActuationError& ActuationError::operator=(const ActuationError& other) =
    default;

ActuationError::ActuationError(ActuationError&& other) = default;
ActuationError& ActuationError::operator=(ActuationError&& other) = default;

std::string GetActuationErrorMessage(const ActuationError& error) {
  if (error.message.has_value()) {
    return error.message.value();
  }
  switch (error.code) {
    case ActuationErrorCode::kUnknown:
      return "Unknown error.";
    case ActuationErrorCode::kUnsupportedAction:
      return "There isn't a tool to support this action.";
    case ActuationErrorCode::kToolDisabledByFeature:
      return "Tool is disabled via feature parameter.";
    case ActuationErrorCode::kExecutionMissingDependencies:
      return "On tool execution, required dependencies were missing.";
    case ActuationErrorCode::kCreationMissingRequiredFields:
      return "On tool creation, required fields were missing.";
    case ActuationErrorCode::kCreationTargetTabNotFound:
      return "On tool creation, target tab isn't in any Browser.";
    case ActuationErrorCode::kCreationMissingWebStateList:
      return "On tool creation, failed to get WebStateList.";
    case ActuationErrorCode::kCreationMissingWebState:
      return "On tool creation, failed to get WebState.";
    case ActuationErrorCode::kJavascriptFeatureGotInvalidResult:
      return "The JavaScriptFeature got an unexpected response from the "
             "JavaScript function.";
    case ActuationErrorCode::kJavascriptFeatureFailedToCallJavaScriptFunction:
      return "The JavaScriptFeature::CallJavaScriptFunction call failed.";
    case ActuationErrorCode::kJavascriptFeatureFailedInJavaScriptExecution:
      return "The JavaScriptFeature failed when executing the JavaScript.";
    case ActuationErrorCode::kNavigationInvalidURL:
      return "Navigation failed due to invalid destination URL.";
    case ActuationErrorCode::kNavigationTabNotRealized:
      return "Navigation failed because the target tab is not realized.";
  }
  NOTREACHED();
}

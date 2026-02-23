// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_ERROR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_ERROR_H_

#import <string>

// Error codes for ActuationService.
enum class ActuationErrorCode {
  // Default error code for unknown or unspecified failures.
  kUnknown,
  // The specific tool type is disabled via the 'DisabledTools' feature
  // parameter.
  kToolDisabled,
  // The action provided in the proto is not recognized or supported.
  kUnsupportedAction,
  // The factory failed to create a valid tool object from the tool data.
  kToolCreationFailed,
  // The tool encountered an error during execution (e.g., invalid
  // parameters, missing dependencies).
  kExecutionFailed,
  // The tool failed during JavaScript execution.
  kJavascriptExecutionFailed,
};

// Represents an error that occurred during actuation.
struct ActuationError {
  ActuationErrorCode code;
  std::string message;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_ERROR_H_

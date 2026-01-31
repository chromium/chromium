// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_ACTUATION_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_ACTUATION_TOOL_H_

#include <string>

#import "base/functional/callback_forward.h"
#import "base/types/expected.h"

// Abstract base class for all actuation tools.
class ActuationTool {
 public:
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
  };

  // Represents an error that occurred during actuation.
  struct ActuationError {
    ActuationErrorCode code;
    std::string message;
  };

  using ActuationResult = base::expected<void, ActuationError>;
  using ActuationCallback = base::OnceCallback<void(ActuationResult)>;

  virtual ~ActuationTool() = default;

  // Executes the tool.
  virtual void Execute(ActuationCallback callback) = 0;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_ACTUATION_TOOL_H_

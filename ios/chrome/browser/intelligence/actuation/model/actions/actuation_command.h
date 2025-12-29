// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTIONS_ACTUATION_COMMAND_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTIONS_ACTUATION_COMMAND_H_

#include <string>

#import "base/functional/callback_forward.h"
#import "base/types/expected.h"

// Abstract base class for all actuation commands.
class ActuationCommand {
 public:
  enum class ActuationErrorCode {
    // Default error code for unknown or unspecified failures.
    kUnknown,
    // The kActuationTools feature flag is disabled.
    kFeatureDisabled,
    // The specific action type is disabled via the 'DisabledActions' feature
    // parameter.
    kActionDisabled,
    // The action type provided in the proto is not recognized or supported.
    kUnknownActionType,
    // The factory failed to create a valid command object from the action data.
    kCommandCreationFailed,
    // The command encountered an error during execution (e.g., invalid
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

  virtual ~ActuationCommand() = default;

  // Executes the command.
  virtual void Execute(ActuationCallback callback) = 0;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTIONS_ACTUATION_COMMAND_H_

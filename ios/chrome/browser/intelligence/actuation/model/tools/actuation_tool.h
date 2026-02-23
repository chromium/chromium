// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_ACTUATION_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_ACTUATION_TOOL_H_

#import "base/functional/callback_forward.h"
#import "base/types/expected.h"

struct ActuationError;

// Abstract base class for all actuation tools.
class ActuationTool {
 public:
  using ActuationResult = base::expected<void, ActuationError>;
  using ActuationCallback = base::OnceCallback<void(ActuationResult)>;

  virtual ~ActuationTool() = default;

  // Executes the tool.
  virtual void Execute(ActuationCallback callback) = 0;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_ACTUATION_TOOL_H_

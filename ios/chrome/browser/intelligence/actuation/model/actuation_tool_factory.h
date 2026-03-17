// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_TOOL_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_TOOL_FACTORY_H_

#import <memory>

#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"

namespace optimization_guide {
namespace proto {
class Action;
}  // namespace proto
}  // namespace optimization_guide

class ProfileIOS;

// Factory for creating ActuationTool objects from raw action data.
class ActuationToolFactory {
 public:
  ActuationToolFactory();
  virtual ~ActuationToolFactory();

  // Creates an ActuationTool based on the provided action proto.
  //
  // This is virtual for testing.
  virtual base::expected<std::unique_ptr<ActuationTool>, ActuationError>
  CreateTool(const optimization_guide::proto::Action& action,
             ProfileIOS* profile);
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_TOOL_FACTORY_H_

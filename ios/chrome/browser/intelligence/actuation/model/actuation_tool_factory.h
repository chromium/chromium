// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_TOOL_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_TOOL_FACTORY_H_

#include <memory>
#include <string>

namespace optimization_guide {
namespace proto {
class Action;
}  // namespace proto
}  // namespace optimization_guide

class ActuationTool;

// Factory for creating ActuationTool objects from raw tool data.
class ActuationToolFactory {
 public:
  ActuationToolFactory();
  ~ActuationToolFactory();

  // Creates an ActuationTool based on the provided tool proto.
  // Returns nullptr if the tool type is unknown or disabled.
  std::unique_ptr<ActuationTool> CreateTool(
      const optimization_guide::proto::Action& tool);
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_TOOL_FACTORY_H_

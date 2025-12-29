// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_ACTION_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_ACTION_FACTORY_H_

#include <memory>
#include <string>

namespace optimization_guide {
namespace proto {
class Action;
}  // namespace proto
}  // namespace optimization_guide

class ActuationCommand;

// Factory for creating ActuationCommand objects from raw action data.
class ActuationActionFactory {
 public:
  ActuationActionFactory();
  ~ActuationActionFactory();

  // Creates an ActuationCommand based on the provided action proto.
  // Returns nullptr if the action type is unknown or disabled.
  std::unique_ptr<ActuationCommand> CreateActionCommand(
      const optimization_guide::proto::Action& action);
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_ACTION_FACTORY_H_

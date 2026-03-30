// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_FACTORY_H_

#import <memory>

#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_error.h"

namespace optimization_guide {
namespace proto {
class Action;
}  // namespace proto
}  // namespace optimization_guide

class ProfileIOS;

// Factory for creating ActorTool objects from raw action data.
class ActorToolFactory {
 public:
  ActorToolFactory();
  virtual ~ActorToolFactory();

  // Creates an ActorTool based on the provided action proto.
  //
  // This is virtual for testing.
  virtual base::expected<std::unique_ptr<ActorTool>, ActorToolError> CreateTool(
      const optimization_guide::proto::Action& action,
      ProfileIOS* profile);
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_FACTORY_H_

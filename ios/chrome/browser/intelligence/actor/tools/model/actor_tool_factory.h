// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_FACTORY_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

class ProfileIOS;

namespace actor {

// Factory for creating ActorTool objects from raw action data.
class ActorToolFactory {
 public:
  explicit ActorToolFactory(ProfileIOS* profile);
  virtual ~ActorToolFactory();

  // Creates an ActorTool based on the provided action proto.
  //
  // This is virtual for testing.
  virtual base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult>
  CreateTool(const optimization_guide::proto::Action& action);

  // Returns the list of supported capabilities by this tool factory.
  virtual std::vector<optimization_guide::proto::Action::ActionCase>
  GetSupportedCapabilities() const;

 private:
  // The profile associated with this factory. This factory is created by the
  // ActorService, a profile-keyed service, which will outlive this.
  raw_ptr<ProfileIOS> profile_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_FACTORY_H_

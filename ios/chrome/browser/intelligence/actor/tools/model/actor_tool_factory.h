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
#import "ios/chrome/browser/intelligence/actor/tools/utils/profile_context_resolver.h"

class ProfileIOS;

namespace actor {

class ToolDelegate;
class ActorToolRequest;

// Factory for creating `ActorTool` objects from raw action data.
class ActorToolFactory {
 public:
  explicit ActorToolFactory(ProfileIOS* profile);
  virtual ~ActorToolFactory();

  // Creates an `ActorTool` based on the provided
  // `optimization_guide::proto::Action` if it's not disabled by feature flag.
  //
  // This is virtual for testing.
  virtual base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult>
  CreateTool(const ActorToolRequest& request, ToolDelegate* tool_delegate);

  // Returns the list of supported capabilities by this tool factory.
  virtual std::vector<optimization_guide::proto::Action::ActionCase>
  GetSupportedCapabilities() const;

 private:
  // A utility class to let tools get data associated with the `ProfileIOS`.
  ProfileContextResolver profile_context_resolver_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_FACTORY_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TEST_CONTROLLABLE_ACTOR_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TEST_CONTROLLABLE_ACTOR_TOOL_H_

#import <memory>
#import <string>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/raw_ref.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/web_state_id.h"

class ProfileIOS;

namespace actor {

class ActorToolRequest;
class ToolDelegate;

// State shared between a test and the `ControllableActorTool`s it creates,
// letting the test complete validation and execution by hand.
struct ControllableToolHooks {
  ControllableToolHooks();
  ~ControllableToolHooks();

  // Completes the pending `Validate()` call when run.
  ToolExecutionCallback validate_callback;
  // Completes the pending `Execute()` call when run.
  ToolExecutionCallback execute_callback;
  // If set, run synchronously from within `Execute()`.
  base::OnceClosure on_execute;
  // Target `WebStateID` of each tool that began `Execute()`.
  std::vector<web::WebStateID> executed_action_ids;
  // `ToolDelegate` passed to the active tool, cleared on tool destruction.
  raw_ptr<ToolDelegate> tool_delegate = nullptr;
  // Whether the most recently created tool has been destroyed.
  bool tool_destroyed = false;
};

// A tool that only completes validation and execution when the test runs the
// callbacks stored in its `ControllableToolHooks`.
class ControllableActorTool : public ActorTool {
 public:
  ControllableActorTool(ControllableToolHooks& hooks,
                        ToolDelegate* tool_delegate,
                        web::WebStateID target_web_state_id);
  ~ControllableActorTool() override;

  // ActorTool:
  void Validate(ToolExecutionCallback callback) override;
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;
  std::string DebugString() const override;

 private:
  const raw_ref<ControllableToolHooks> hooks_;
  const web::WebStateID target_web_state_id_;
};

// Creates `ControllableActorTool`s bound to `hooks`.
class ControllableActorToolFactory : public ActorToolFactory {
 public:
  ControllableActorToolFactory(ProfileIOS* profile,
                               ControllableToolHooks& hooks);
  ~ControllableActorToolFactory() override;

  // ActorToolFactory:
  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> CreateTool(
      const ActorToolRequest& request,
      ToolDelegate* tool_delegate) override;

 private:
  const raw_ref<ControllableToolHooks> hooks_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TEST_CONTROLLABLE_ACTOR_TOOL_H_

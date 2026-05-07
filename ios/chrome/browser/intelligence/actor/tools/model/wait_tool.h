// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_WAIT_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_WAIT_TOOL_H_

#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

class ProfileIOS;

namespace optimization_guide {
namespace proto {
class WaitAction;
}  // namespace proto
}  // namespace optimization_guide

namespace web {
class WebState;
}  // namespace web

namespace actor {

struct ToolExecutionResult;

// Tool to wait for a duration.
class WaitTool : public ActorTool {
 public:
  ~WaitTool() override;

  // Creates the tool using the given `action`.
  static base::expected<std::unique_ptr<WaitTool>, ToolExecutionResult> Create(
      const optimization_guide::proto::WaitAction& action,
      ProfileIOS* profile);

  // ActorTool:
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  optimization_guide::proto::Action::ActionCase GetActionCase() const override;

 private:
  WaitTool(base::TimeDelta wait_duration,
           base::WeakPtr<web::WebState> observe_web_state);

  // Callback method to be executed once the wait duration has passed.
  void OnDelayFinished(ToolExecutionCallback callback);

  // Duration to wait for.
  base::TimeDelta wait_duration_;

  // An observation from this web state will be observed (but not
  // controlled) by the actor task.
  base::WeakPtr<web::WebState> observe_web_state_;

  base::WeakPtrFactory<WaitTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_WAIT_TOOL_H_

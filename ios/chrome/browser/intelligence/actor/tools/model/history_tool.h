// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_HISTORY_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_HISTORY_TOOL_H_

#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

class ProfileIOS;

namespace optimization_guide::proto {
class HistoryBackAction;
class HistoryForwardAction;
}  // namespace optimization_guide::proto

namespace web {
class WebState;
}  // namespace web

namespace actor {

struct ToolExecutionResult;

// Tool to navigate back or forward in a tab's history.
class HistoryTool : public ActorTool {
 public:
  ~HistoryTool() override;

  // Create the tool to handle "go back" action.
  static base::expected<std::unique_ptr<HistoryTool>, ToolExecutionResult>
  Create(const optimization_guide::proto::HistoryBackAction& action,
         ProfileIOS* profile);

  // Create the tool to handle "go forward" action.
  static base::expected<std::unique_ptr<HistoryTool>, ToolExecutionResult>
  Create(const optimization_guide::proto::HistoryForwardAction& action,
         ProfileIOS* profile);

  // ActorTool:
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  optimization_guide::proto::Action::ActionCase GetActionCase() const override;

 private:
  // Internal helper to create the public `Create` method.
  template <typename HistoryAction>
  static base::expected<std::unique_ptr<HistoryTool>, ToolExecutionResult>
  CreateInternal(const HistoryAction& action, ProfileIOS* profile);

  HistoryTool(bool is_back_action, base::WeakPtr<web::WebState> web_state);

  bool is_back_action_;
  base::WeakPtr<web::WebState> web_state_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_HISTORY_TOOL_H_

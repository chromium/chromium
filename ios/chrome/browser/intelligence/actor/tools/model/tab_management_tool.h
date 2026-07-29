// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TAB_MANAGEMENT_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TAB_MANAGEMENT_TOOL_H_

#import <memory>

#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

class WebStateList;

namespace web {
class WebState;
}

namespace actor {

// A tool to perform tab management operations.
class TabManagementTool : public ActorTool {
 public:
  // Creates a TabManagementTool for the CloseTab action.
  static std::unique_ptr<TabManagementTool> CreateCloseTabTool(
      base::WeakPtr<web::WebState> web_state,
      base::WeakPtr<WebStateList> web_state_list);

  ~TabManagementTool() override;

  // ActorTool:
  void Validate(ToolExecutionCallback callback) override;
  void Execute(ToolExecutionCallback callback) override;
  void Cancel() override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;

 private:
  enum class ActionType {
    kClose,
  };

  TabManagementTool(base::WeakPtr<web::WebState> web_state,
                    ActionType action_type,
                    base::WeakPtr<WebStateList> web_state_list);

  void ValidateCloseTab(ToolExecutionCallback callback);
  void ExecuteCloseTab();

  const ActionType action_type_;
  const base::WeakPtr<web::WebState> web_state_;
  const base::WeakPtr<WebStateList> web_state_list_;

  ToolExecutionCallback callback_;

  base::WeakPtrFactory<TabManagementTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TAB_MANAGEMENT_TOOL_H_

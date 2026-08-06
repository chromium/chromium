// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TAB_MANAGEMENT_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TAB_MANAGEMENT_TOOL_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

class WebStateList;

namespace optimization_guide {
namespace proto {
class CreateTabAction;
}  // namespace proto
}  // namespace optimization_guide

namespace web {
class WebState;
}

namespace actor {

class ToolDelegate;

// A tool to perform tab management operations.
class TabManagementTool : public ActorTool {
 public:
  // Creates a TabManagementTool for the CloseTab action.
  static std::unique_ptr<TabManagementTool> CreateCloseTabTool(
      base::WeakPtr<web::WebState> web_state,
      base::WeakPtr<WebStateList> web_state_list);

  // Creates a TabManagementTool for the ActivateTab action.
  static std::unique_ptr<TabManagementTool> CreateActivateTabTool(
      base::WeakPtr<web::WebState> web_state,
      base::WeakPtr<WebStateList> web_state_list);

  // Creates a TabManagementTool for the CreateTab action.
  static std::unique_ptr<TabManagementTool> CreateTabTool(
      const optimization_guide::proto::CreateTabAction& action,
      ToolDelegate* tool_delegate);

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
    kCreate,
    kActivate,
  };

  TabManagementTool(base::WeakPtr<web::WebState> web_state,
                    ActionType action_type,
                    base::WeakPtr<WebStateList> web_state_list);
  TabManagementTool(int32_t window_id,
                    bool foreground,
                    ToolDelegate* tool_delegate);

  // Validates that the targeted tab and web state list exist and are valid.
  ToolExecutionResult ValidateTabExists() const;

  // Validates that the target window is valid for tab creation.
  ToolExecutionResult ValidateCreateTab() const;

  void ExecuteCloseTab();
  void ExecuteActivateTab();
  void ExecuteCreateTab();

  const ActionType action_type_;
  const base::WeakPtr<web::WebState> web_state_;
  const base::WeakPtr<WebStateList> web_state_list_;
  const raw_ptr<ToolDelegate> tool_delegate_ = nullptr;
  const int32_t window_id_ = -1;
  const bool foreground_;

  ToolExecutionCallback callback_;

  base::WeakPtr<web::WebState> created_web_state_;
  base::WeakPtrFactory<TabManagementTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TAB_MANAGEMENT_TOOL_H_

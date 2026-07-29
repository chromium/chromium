// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/tab_management_tool.h"

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/task/sequenced_task_runner.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_browser_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

namespace actor {

// static
std::unique_ptr<TabManagementTool> TabManagementTool::CreateCloseTabTool(
    base::WeakPtr<web::WebState> web_state,
    base::WeakPtr<WebStateList> web_state_list) {
  return std::unique_ptr<TabManagementTool>(
      new TabManagementTool(web_state, ActionType::kClose, web_state_list));
}

TabManagementTool::~TabManagementTool() = default;

void TabManagementTool::Validate(ToolExecutionCallback callback) {
  switch (action_type_) {
    case ActionType::kClose:
      ValidateCloseTab(std::move(callback));
      break;
  }
}

void TabManagementTool::ValidateCloseTab(ToolExecutionCallback callback) {
  CHECK_EQ(action_type_, ActionType::kClose);
  if (!web_state_list_) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kWindowWentAway));
    return;
  }
  if (!web_state_) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }
  if (web_state_list_->GetIndexOfWebState(web_state_.get()) ==
      WebStateList::kInvalidIndex) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }
  std::move(callback).Run(ToolExecutionResult::Ok());
}

void TabManagementTool::Execute(ToolExecutionCallback callback) {
  callback_ = std::move(callback);
  switch (action_type_) {
    case ActionType::kClose:
      ExecuteCloseTab();
      break;
  }
}

void TabManagementTool::ExecuteCloseTab() {
  CHECK_EQ(action_type_, ActionType::kClose);
  if (!web_state_list_) {
    std::move(callback_).Run(
        ToolExecutionResult(mojom::ActionResultCode::kWindowWentAway));
    return;
  }

  if (!web_state_) {
    std::move(callback_).Run(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  int index = web_state_list_->GetIndexOfWebState(web_state_.get());
  if (index == WebStateList::kInvalidIndex) {
    std::move(callback_).Run(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  // `CloseWebStateAt` may trigger synchronous destruction of `this`. Holding
  // the callback on the stack guarantees it survives even if `this` is deleted.
  ToolExecutionCallback local_callback = std::move(callback_);
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  web_state_list_->CloseWebStateAt(index,
                                   WebStateList::ClosingReason::kUserAction);
  // Check that `this ` was not destroyed before executing any more code.
  if (!weak_this) {
    return;
  }
  std::move(local_callback).Run(ToolExecutionResult::Ok());
}

void TabManagementTool::Cancel() {
  if (callback_) {
    std::move(callback_).Run(
        ToolExecutionResult(mojom::ActionResultCode::kActionsCancelled));
  }
}

base::WeakPtr<web::WebState> TabManagementTool::GetTargetWebState() const {
  return web_state_;
}

ToolType TabManagementTool::GetToolType() const {
  switch (action_type_) {
    case ActionType::kClose:
      return ToolType::kCloseTab;
  }
}

TabManagementTool::TabManagementTool(base::WeakPtr<web::WebState> web_state,
                                     ActionType action_type,
                                     base::WeakPtr<WebStateList> web_state_list)
    : action_type_(action_type),
      web_state_(web_state),
      web_state_list_(web_state_list) {}

}  // namespace actor

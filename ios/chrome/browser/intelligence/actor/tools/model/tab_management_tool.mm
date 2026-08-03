// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/tab_management_tool.h"

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/task/sequenced_task_runner.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_browser_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"
#import "url/url_constants.h"

namespace actor {

// static
std::unique_ptr<TabManagementTool> TabManagementTool::CreateCloseTabTool(
    base::WeakPtr<web::WebState> web_state,
    base::WeakPtr<WebStateList> web_state_list) {
  return std::unique_ptr<TabManagementTool>(
      new TabManagementTool(web_state, ActionType::kClose, web_state_list));
}

// static
std::unique_ptr<TabManagementTool> TabManagementTool::CreateTabTool(
    const optimization_guide::proto::CreateTabAction& action,
    ToolDelegate* tool_delegate) {
  // Default to foreground when unspecified, following Desktop's example:
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/actor/actor_proto_conversion.cc;l=437;drc=853aabf2c9c58f9e0389c13ed41d843806dca3b9
  bool open_in_foreground = !action.has_foreground() || action.foreground();
  return std::unique_ptr<TabManagementTool>(new TabManagementTool(
      action.window_id(), open_in_foreground, tool_delegate));
}

TabManagementTool::~TabManagementTool() = default;

void TabManagementTool::Validate(ToolExecutionCallback callback) {
  switch (action_type_) {
    case ActionType::kClose:
      ValidateCloseTab(std::move(callback));
      break;
    case ActionType::kCreate:
      ValidateCreateTab(std::move(callback));
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

void TabManagementTool::ValidateCreateTab(ToolExecutionCallback callback) {
  CHECK_EQ(action_type_, ActionType::kCreate);
  if (!tool_delegate_->IsWindowIdValid(window_id_)) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kWindowWentAway));
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
    case ActionType::kCreate:
      ExecuteCreateTab();
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

void TabManagementTool::ExecuteCreateTab() {
  CHECK_EQ(action_type_, ActionType::kCreate);
  if (!tool_delegate_->IsWindowIdValid(window_id_)) {
    std::move(callback_).Run(
        ToolExecutionResult(mojom::ActionResultCode::kWindowWentAway));
    return;
  }
  web::NavigationManager::WebLoadParams load_params{GURL(url::kAboutBlankURL)};
  ToolExecutionCallback local_callback = std::move(callback_);
  base::WeakPtr<TabManagementTool> weak_this = weak_ptr_factory_.GetWeakPtr();
  web::WebState* web_state =
      tool_delegate_->InsertWebState(window_id_, load_params, !foreground_);
  if (!weak_this) {
    return;
  }

  if (!web_state) {
    std::move(local_callback)
        .Run(ToolExecutionResult(
            mojom::ActionResultCode::kNewTabCreationFailed));
    return;
  }

  created_web_state_ = web_state->GetWeakPtr();
  std::move(local_callback).Run(ToolExecutionResult::Ok());
}

void TabManagementTool::Cancel() {
  if (callback_) {
    std::move(callback_).Run(
        ToolExecutionResult(mojom::ActionResultCode::kActionsCancelled));
  }
}

base::WeakPtr<web::WebState> TabManagementTool::GetTargetWebState() const {
  if (action_type_ == ActionType::kCreate) {
    return created_web_state_;
  }
  return web_state_;
}

ToolType TabManagementTool::GetToolType() const {
  switch (action_type_) {
    case ActionType::kCreate:
      return ToolType::kCreateTab;
    case ActionType::kClose:
      return ToolType::kCloseTab;
  }
}

TabManagementTool::TabManagementTool(base::WeakPtr<web::WebState> web_state,
                                     ActionType action_type,
                                     base::WeakPtr<WebStateList> web_state_list)
    : action_type_(action_type),
      web_state_(web_state),
      web_state_list_(web_state_list),
      tool_delegate_(nullptr),
      window_id_(-1),
      foreground_(false) {}

TabManagementTool::TabManagementTool(int32_t window_id,
                                     bool foreground,
                                     ToolDelegate* tool_delegate)
    : action_type_(ActionType::kCreate),
      web_state_(nullptr),
      web_state_list_(nullptr),
      tool_delegate_(tool_delegate),
      window_id_(window_id),
      foreground_(foreground) {
  CHECK(tool_delegate_);
}

}  // namespace actor

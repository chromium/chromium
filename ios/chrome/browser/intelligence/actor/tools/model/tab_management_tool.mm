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
#import "ios/chrome/browser/intelligence/actor/tools/utils/profile_context_resolver.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

namespace actor {

// static
base::expected<std::unique_ptr<TabManagementTool>, ToolExecutionResult>
TabManagementTool::CreateCloseTabTool(
    const optimization_guide::proto::CloseTabAction& action,
    const ProfileContextResolver& profile_context_resolver) {
  base::expected<ProfileContextResolver::TabResolutionResult,
                 ToolExecutionResult>
      resolution_result = profile_context_resolver.ResolveTab(action.tab_id());
  if (!resolution_result.has_value()) {
    return base::unexpected(resolution_result.error());
  }

  return std::unique_ptr<TabManagementTool>(new TabManagementTool(
      ActionType::kClose, resolution_result.value().web_state,
      resolution_result.value().web_state_list));
}

TabManagementTool::~TabManagementTool() = default;

void TabManagementTool::Execute(ToolExecutionCallback callback) {
  callback_ = std::move(callback);
  switch (action_type_) {
    case ActionType::kClose: {
      if (!web_state_list_) {
        // Run callback asynchronously to prevent synchronous destruction of
        // `this`.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                std::move(callback_),
                ToolExecutionResult(mojom::ActionResultCode::kWindowWentAway)));
        return;
      }

      if (!web_state_) {
        // Run callback asynchronously to prevent synchronous destruction of
        // `this`.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                std::move(callback_),
                ToolExecutionResult(mojom::ActionResultCode::kTabWentAway)));
        return;
      }

      int index = web_state_list_->GetIndexOfWebState(web_state_.get());
      if (index == WebStateList::kInvalidIndex) {
        // Run callback asynchronously to prevent synchronous destruction of
        // `this`.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                std::move(callback_),
                ToolExecutionResult(mojom::ActionResultCode::kTabWentAway)));
        return;
      }

      // Move the callback to the stack beforehand. Performing `CloseWebStateAt`
      // can trigger synchronous destruction of `this`. Holding the callback on
      // the stack guarantees it survives even if `this` is deleted.
      ToolExecutionCallback local_callback = std::move(callback_);
      web_state_list_->CloseWebStateAt(
          index, WebStateList::ClosingReason::kUserAction);
      // Run callback asynchronously to prevent synchronous destruction of
      // `this`.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(local_callback), ToolExecutionResult::Ok()));
      break;
    }
  }
}

void TabManagementTool::Cancel() {
  if (callback_) {
    // Run callback asynchronously to prevent synchronous destruction of `this`.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback_),
            ToolExecutionResult(mojom::ActionResultCode::kActionsCancelled)));
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

TabManagementTool::TabManagementTool(ActionType action_type,
                                     base::WeakPtr<web::WebState> web_state,
                                     base::WeakPtr<WebStateList> web_state_list)
    : action_type_(action_type),
      web_state_(web_state),
      web_state_list_(web_state_list) {}

}  // namespace actor

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/history_tool.h"

#import <type_traits>

#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace actor {

HistoryTool::~HistoryTool() = default;

// static
base::expected<std::unique_ptr<HistoryTool>, ToolExecutionResult>
HistoryTool::Create(const optimization_guide::proto::HistoryBackAction& action,
                    ProfileIOS* profile) {
  return CreateInternal(action, profile);
}

// static
base::expected<std::unique_ptr<HistoryTool>, ToolExecutionResult>
HistoryTool::Create(
    const optimization_guide::proto::HistoryForwardAction& action,
    ProfileIOS* profile) {
  return CreateInternal(action, profile);
}

void HistoryTool::Execute(ToolExecutionCallback callback) {
  if (!web_state_ || !web_state_->IsRealized() ||
      !web_state_->GetNavigationManager()) {
    std::move(callback).Run(ToolExecutionResult(
        InternalToolErrorCode::kExecutionMissingDependencies));
    return;
  }

  web::NavigationManager* navigation_manager =
      web_state_->GetNavigationManager();
  if (is_back_action_) {
    if (navigation_manager->CanGoBack()) {
      navigation_manager->GoBack();
      std::move(callback).Run(ToolExecutionResult::Ok());
    } else {
      std::move(callback).Run(
          ToolExecutionResult(InternalToolErrorCode::kHistoryBackNotPossible));
    }
  } else {
    if (navigation_manager->CanGoForward()) {
      navigation_manager->GoForward();
      std::move(callback).Run(ToolExecutionResult::Ok());
    } else {
      std::move(callback).Run(ToolExecutionResult(
          InternalToolErrorCode::kHistoryForwardNotPossible));
    }
  }
}

base::WeakPtr<web::WebState> HistoryTool::GetTargetWebState() const {
  return web_state_;
}

optimization_guide::proto::Action::ActionCase HistoryTool::GetActionCase()
    const {
  return is_back_action_ ? optimization_guide::proto::Action::kBack
                         : optimization_guide::proto::Action::kForward;
}

// static
template <typename HistoryAction>
base::expected<std::unique_ptr<HistoryTool>, ToolExecutionResult>
HistoryTool::CreateInternal(const HistoryAction& action, ProfileIOS* profile) {
  if (!action.has_tab_id()) {
    return base::unexpected(ToolExecutionResult(
        InternalToolErrorCode::kCreationMissingRequiredFields));
  }
  auto resolution_result = ResolveTab(action.tab_id(), profile);
  if (!resolution_result.has_value()) {
    return base::unexpected(resolution_result.error());
  }
  constexpr bool is_back_action =
      std::is_same_v<HistoryAction,
                     optimization_guide::proto::HistoryBackAction>;
  return std::unique_ptr<HistoryTool>(
      new HistoryTool(is_back_action, resolution_result.value().web_state));
}

HistoryTool::HistoryTool(bool is_back_action,
                         base::WeakPtr<web::WebState> web_state)
    : is_back_action_(is_back_action), web_state_(web_state) {}

}  // namespace actor

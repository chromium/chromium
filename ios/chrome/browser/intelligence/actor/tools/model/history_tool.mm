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
std::unique_ptr<HistoryTool> HistoryTool::Create(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::HistoryBackAction& action) {
  return std::unique_ptr<HistoryTool>(
      new HistoryTool(web_state, /*is_back_action=*/true));
}

// static
std::unique_ptr<HistoryTool> HistoryTool::Create(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::HistoryForwardAction& action) {
  return std::unique_ptr<HistoryTool>(
      new HistoryTool(web_state, /*is_back_action=*/false));
}

void HistoryTool::Validate(ToolExecutionCallback callback) {
  std::move(callback).Run(ToolExecutionResult::Ok());
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

ToolType HistoryTool::GetToolType() const {
  return is_back_action_ ? ToolType::kBack : ToolType::kForward;
}

HistoryTool::HistoryTool(base::WeakPtr<web::WebState> web_state,
                         bool is_back_action)
    : is_back_action_(is_back_action), web_state_(web_state) {}

}  // namespace actor

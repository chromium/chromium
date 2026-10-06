// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/history_tool.h"

#import <type_traits>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/time/time.h"
#import "base/types/expected.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace actor {

namespace {
constexpr base::TimeDelta kNavigationTimeout = base::Seconds(10);
}  // namespace

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
  invoke_callback_ = std::move(callback);

  if (!web_state_ || !web_state_->IsRealized() ||
      !web_state_->GetNavigationManager()) {
    std::move(invoke_callback_)
        .Run(ToolExecutionResult(
            InternalToolErrorCode::kExecutionMissingDependencies));
    return;
  }

  web::NavigationManager* navigation_manager =
      web_state_->GetNavigationManager();
  if (is_back_action_ && !navigation_manager->CanGoBack()) {
    std::move(invoke_callback_)
        .Run(ToolExecutionResult(
            InternalToolErrorCode::kHistoryBackNotPossible));
    return;
  }

  if (!is_back_action_ && !navigation_manager->CanGoForward()) {
    std::move(invoke_callback_)
        .Run(ToolExecutionResult(
            InternalToolErrorCode::kHistoryForwardNotPossible));
    return;
  }

  web_state_observation_.Observe(web_state_.get());
  navigation_timeout_timer_.Start(
      FROM_HERE, kNavigationTimeout,
      base::BindOnce(&HistoryTool::OnNavigationTimeout,
                     weak_ptr_factory_.GetWeakPtr()));

  if (is_back_action_) {
    navigation_manager->GoBack();
  } else {
    navigation_manager->GoForward();
  }
}

void HistoryTool::Cancel() {
  navigation_timeout_timer_.Stop();
  web_state_observation_.Reset();
  pending_navigation_id_.reset();
  invoke_callback_.Reset();
}

base::WeakPtr<web::WebState> HistoryTool::GetTargetWebState() const {
  return web_state_;
}

ToolType HistoryTool::GetToolType() const {
  return is_back_action_ ? ToolType::kBack : ToolType::kForward;
}

std::string HistoryTool::DebugString() const {
  return is_back_action_ ? "HistoryTool[Back]" : "HistoryTool[Forward]";
}

void HistoryTool::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (pending_navigation_id_.has_value() || !navigation_context ||
      navigation_context->IsSameDocument() ||
      navigation_context->IsRendererInitiated()) {
    return;
  }

  pending_navigation_id_ = navigation_context->GetNavigationId();
}

void HistoryTool::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!pending_navigation_id_.has_value() || !navigation_context ||
      navigation_context->GetNavigationId() != *pending_navigation_id_) {
    return;
  }

  navigation_timeout_timer_.Stop();
  web_state_observation_.Reset();
  pending_navigation_id_.reset();

  if (!invoke_callback_) {
    return;
  }

  ToolExecutionResult result = ToolExecutionResult::Ok();
  if (!navigation_context->HasCommitted()) {
    result = ToolExecutionResult(
        mojom::ActionResultCode::kHistoryFailedBeforeCommit);
  } else if (navigation_context->GetError() != nil) {
    result = ToolExecutionResult(mojom::ActionResultCode::kHistoryErrorPage);
  }

  std::move(invoke_callback_).Run(std::move(result));
}

void HistoryTool::WebStateDestroyed(web::WebState* web_state) {
  navigation_timeout_timer_.Stop();
  web_state_observation_.Reset();
  pending_navigation_id_.reset();
  if (invoke_callback_) {
    std::move(invoke_callback_)
        .Run(ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
  }
}

void HistoryTool::OnNavigationTimeout() {
  web_state_observation_.Reset();
  const bool navigation_started = pending_navigation_id_.has_value();
  pending_navigation_id_.reset();
  if (invoke_callback_) {
    std::move(invoke_callback_)
        .Run(ToolExecutionResult(
            navigation_started
                ? mojom::ActionResultCode::kToolTimeout
                : mojom::ActionResultCode::kHistoryNoNavigationsCreated));
  }
}

HistoryTool::HistoryTool(base::WeakPtr<web::WebState> web_state,
                         bool is_back_action)
    : is_back_action_(is_back_action), web_state_(web_state) {}

}  // namespace actor

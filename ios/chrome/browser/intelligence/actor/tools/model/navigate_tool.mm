// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/navigate_tool.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/time/time.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace actor {

namespace {
constexpr base::TimeDelta kNavigationTimeout = base::Seconds(10);
}  // namespace

// static
std::unique_ptr<NavigateTool> NavigateTool::Create(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::NavigateAction& action,
    base::WeakPtr<UrlLoadingBrowserAgent> url_loader,
    ToolDelegate* tool_delegate) {
  std::optional<std::string> url = std::nullopt;
  if (action.has_url()) {
    url = action.url();
  }
  return std::unique_ptr<NavigateTool>(
      new NavigateTool(web_state, url, url_loader, tool_delegate));
}

void NavigateTool::Validate(ToolExecutionCallback callback) {
  if (!url_.has_value()) {
    std::move(callback).Run(ToolExecutionResult(
        InternalToolErrorCode::kCreationMissingRequiredFields));
    return;
  }
  const GURL destination_url(url_.value());
  if (!destination_url.is_valid()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kNavigateInvalidUrl));
    return;
  }
  if (!tool_delegate_) {
    std::move(callback).Run(ToolExecutionResult(
        InternalToolErrorCode::kExecutionMissingDependencies));
    return;
  }
  tool_delegate_->IsAcceptableNavigationDestination(destination_url,
                                                    std::move(callback));
}

NavigateTool::NavigateTool(base::WeakPtr<web::WebState> web_state,
                           std::optional<std::string> url,
                           base::WeakPtr<UrlLoadingBrowserAgent> url_loader,
                           ToolDelegate* tool_delegate)
    : url_(url),
      web_state_(web_state),
      url_loader_(url_loader),
      tool_delegate_(tool_delegate) {}

NavigateTool::~NavigateTool() = default;

void NavigateTool::Execute(ToolExecutionCallback callback) {
  invoke_callback_ = std::move(callback);

  if (!web_state_ || !url_loader_) {
    std::move(invoke_callback_)
        .Run(ToolExecutionResult(
            InternalToolErrorCode::kExecutionMissingDependencies));
    return;
  }

  const GURL destination_url(url_.value_or(""));
  if (!destination_url.is_valid()) {
    std::move(invoke_callback_)
        .Run(mojom::ActionResultCode::kNavigateInvalidUrl);
    return;
  }

  // Unrealized WebStates are restored, but not fully functional, tabs that
  // haven't been activated yet. They do not support navigation.
  if (!web_state_->IsRealized()) {
    std::move(invoke_callback_)
        .Run(ToolExecutionResult(
            InternalToolErrorCode::kNavigationTabNotRealized));
    return;
  }

  web_state_observation_.Observe(web_state_.get());
  navigation_timeout_timer_.Start(
      FROM_HERE, kNavigationTimeout,
      base::BindOnce(&NavigateTool::OnNavigationTimeout,
                     weak_ptr_factory_.GetWeakPtr()));

  UrlLoadParams params = UrlLoadParams::InCurrentTab(destination_url);
  params.from_chrome = true;
  params.user_initiated = false;
  params.web_params.transition_type =
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL;
  params.web_params.is_renderer_initiated = false;
  url_loader_->LoadUrlInTab(params, web_state_.get());
}

void NavigateTool::OnNavigationTimeout() {
  web_state_observation_.Reset();
  const bool navigation_started = pending_navigation_id_.has_value();
  pending_navigation_id_.reset();
  if (invoke_callback_) {
    std::move(invoke_callback_)
        .Run(ToolExecutionResult(
            navigation_started
                ? mojom::ActionResultCode::kToolTimeout
                : mojom::ActionResultCode::kNavigateFailedToStart));
  }
}

base::WeakPtr<web::WebState> NavigateTool::GetTargetWebState() const {
  return web_state_;
}

ToolType NavigateTool::GetToolType() const {
  return ToolType::kNavigate;
}

std::string NavigateTool::DebugString() const {
  return "NavigateTool[" + url_.value_or("") + "]";
}

void NavigateTool::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (pending_navigation_id_.has_value() || !navigation_context ||
      navigation_context->IsSameDocument() ||
      navigation_context->IsRendererInitiated() ||
      navigation_context->GetUrl() != GURL(url_.value_or(""))) {
    return;
  }

  pending_navigation_id_ = navigation_context->GetNavigationId();
}

void NavigateTool::DidFinishNavigation(
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

  const bool succeeded = navigation_context->HasCommitted() &&
                         navigation_context->GetError() == nil;
  ToolExecutionResult result =
      succeeded ? ToolExecutionResult::Ok()
                : ToolExecutionResult(
                      mojom::ActionResultCode::kNavigateCommittedErrorPage);

  std::move(invoke_callback_).Run(std::move(result));
}

void NavigateTool::Cancel() {
  navigation_timeout_timer_.Stop();
  web_state_observation_.Reset();
  pending_navigation_id_.reset();
  invoke_callback_.Reset();
}

void NavigateTool::WebStateDestroyed(web::WebState* web_state) {
  navigation_timeout_timer_.Stop();
  web_state_observation_.Reset();
  pending_navigation_id_.reset();
  if (invoke_callback_) {
    std::move(invoke_callback_)
        .Run(ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
  }
}

}  // namespace actor

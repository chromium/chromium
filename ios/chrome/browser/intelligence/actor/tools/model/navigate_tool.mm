// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/navigate_tool.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/origin_gating/core/origin_gating_checker.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace actor {

// static
std::unique_ptr<NavigateTool> NavigateTool::Create(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::NavigateAction& action,
    base::WeakPtr<UrlLoadingBrowserAgent> url_loader,
    origin_gating::OriginGatingChecker* gating_checker) {
  std::optional<std::string> url = std::nullopt;
  if (action.has_url()) {
    url = action.url();
  }
  return std::unique_ptr<NavigateTool>(
      new NavigateTool(web_state, url, url_loader, gating_checker));
}

void NavigateTool::Validate(ToolExecutionCallback callback) {
  if (!url_.has_value()) {
    std::move(callback).Run(ToolExecutionResult(
        InternalToolErrorCode::kCreationMissingRequiredFields));
    return;
  }
  std::move(callback).Run(ToolExecutionResult::Ok());
}

NavigateTool::NavigateTool(base::WeakPtr<web::WebState> web_state,
                           std::optional<std::string> url,
                           base::WeakPtr<UrlLoadingBrowserAgent> url_loader,
                           origin_gating::OriginGatingChecker* gating_checker)
    : url_(url),
      web_state_(web_state),
      url_loader_(url_loader),
      gating_checker_(gating_checker) {}

NavigateTool::~NavigateTool() = default;

// TODO(crbug.com/474383578): Limit what URLs can be navigated to using the
// ActorService.
void NavigateTool::Execute(ToolExecutionCallback callback) {
  if (!web_state_ || !url_loader_) {
    std::move(callback).Run(ToolExecutionResult(
        InternalToolErrorCode::kExecutionMissingDependencies));
    return;
  }

  const GURL destination_url(url_.value_or(""));
  if (!destination_url.is_valid()) {
    std::move(callback).Run(
        ToolExecutionResult(InternalToolErrorCode::kNavigationInvalidURL));
    return;
  }

  // Unrealized WebStates are restored, but not fully functional, tabs that
  // haven't been activated yet. They do not support navigation.
  if (!web_state_->IsRealized()) {
    std::move(callback).Run(
        ToolExecutionResult(InternalToolErrorCode::kNavigationTabNotRealized));
    return;
  }

  if (!IsActorOriginGatingForExplicitNavigationEnabled()) {
    // If the feature is disabled, bypass origin gating.
    LoadUrl(destination_url, std::move(callback));
    return;
  }

  // Feature is enabled: checker is required.
  CHECK(gating_checker_);

  const GURL source_url = web_state_->GetLastCommittedURL();
  auto context = std::make_unique<origin_gating::GatingDecisionContext>();
  gating_checker_->ComputeGatingDecision(
      std::move(context), origin_gating::GateableEvent::kNavigationRequest,
      source_url, destination_url,
      base::BindOnce(&NavigateTool::OnGatingDecisionComputed,
                     weak_ptr_factory_.GetWeakPtr(), destination_url,
                     std::move(callback)));
  return;
}

void NavigateTool::OnGatingDecisionComputed(
    const GURL& destination_url,
    ToolExecutionCallback callback,
    std::unique_ptr<origin_gating::GatingDecisionContext> context,
    origin_gating::GatingDecision decision) {
  if (!decision.is_allowed) {
    std::move(callback).Run(ToolExecutionResult(
        mojom::ActionResultCode::kTriggeredNavigationBlocked));
    return;
  }
  LoadUrl(destination_url, std::move(callback));
}

void NavigateTool::LoadUrl(const GURL& destination_url,
                           ToolExecutionCallback callback) {
  // Guard dependencies in case they were invalidated during the async check.
  if (!web_state_ || !url_loader_) {
    std::move(callback).Run(ToolExecutionResult(
        InternalToolErrorCode::kExecutionMissingDependencies));
    return;
  }

  UrlLoadParams params = UrlLoadParams::InCurrentTab(destination_url);
  params.from_chrome = true;
  params.user_initiated = false;
  params.web_params.transition_type =
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL;
  params.web_params.is_renderer_initiated = false;
  url_loader_->LoadUrlInTab(params, web_state_.get());
  std::move(callback).Run(ToolExecutionResult::Ok());
}

base::WeakPtr<web::WebState> NavigateTool::GetTargetWebState() const {
  return web_state_;
}

ToolType NavigateTool::GetToolType() const {
  return ToolType::kNavigate;
}

}  // namespace actor

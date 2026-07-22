// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/navigate_tool.h"

#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
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
    base::WeakPtr<UrlLoadingBrowserAgent> url_loader) {
  std::optional<std::string> url = std::nullopt;
  if (action.has_url()) {
    url = action.url();
  }
  return std::unique_ptr<NavigateTool>(
      new NavigateTool(web_state, url, url_loader));
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
                           base::WeakPtr<UrlLoadingBrowserAgent> url_loader)
    : url_(url), web_state_(web_state), url_loader_(url_loader) {}

NavigateTool::~NavigateTool() = default;

// TODO(crbug.com/474383578): Limit what URLs can be navigated to using the
// ActorService.
void NavigateTool::Execute(ToolExecutionCallback callback) {
  if (!web_state_ || !url_loader_) {
    std::move(callback).Run(ToolExecutionResult(
        InternalToolErrorCode::kExecutionMissingDependencies));
    return;
  }

  GURL url(url_.value_or(""));
  if (!url.is_valid()) {
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

  // These params are selected to align with
  // chrome/browser/actor/tools/navigate_tool.cc.
  UrlLoadParams params = UrlLoadParams::InCurrentTab(url);
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

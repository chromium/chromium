// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/tools/navigate_tool.h"

#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

NavigateTool::~NavigateTool() = default;

// static
base::expected<std::unique_ptr<NavigateTool>, ActuationError>
NavigateTool::Create(const optimization_guide::proto::NavigateAction& action,
                     ProfileIOS* profile) {
  if (!action.has_tab_id() || !action.has_url()) {
    return base::unexpected(
        ActuationError{ActuationErrorCode::kCreationMissingRequiredFields});
  }

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  BrowserAndIndex browser_and_index = FindBrowserAndIndex(
      web::WebStateID::FromSerializedValue(action.tab_id()),
      browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular));
  int tab_index = browser_and_index.tab_index;
  Browser* browser = browser_and_index.browser;
  if (tab_index == WebStateList::kInvalidIndex || !browser) {
    return base::unexpected(
        ActuationError{ActuationErrorCode::kCreationTargetTabNotFound});
  }

  WebStateList* web_state_list = browser->GetWebStateList();
  if (!web_state_list) {
    return base::unexpected(
        ActuationError{ActuationErrorCode::kCreationMissingWebStateList});
  }
  web::WebState* web_state = web_state_list->GetWebStateAt(tab_index);
  if (!web_state) {
    return base::unexpected(
        ActuationError{ActuationErrorCode::kCreationMissingWebState});
  }
  return std::unique_ptr<NavigateTool>(
      new NavigateTool(action.url(), web_state->GetWeakPtr(), web_state_list,
                       UrlLoadingBrowserAgent::FromBrowser(browser)));
}

// TODO(crbug.com/474383578): Limit what URLs can be navigated to using the
// ActuationService.
void NavigateTool::Execute(ActuationCallback callback) {
  if (!web_state_ || !web_state_list_ || !url_loader_) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kExecutionMissingDependencies}));
    return;
  }

  GURL url(url_);
  if (!url.is_valid()) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kNavigationInvalidURL}));
    return;
  }

  // Unrealized WebStates are restored, but not fully functional, tabs that
  // haven't been activated yet. They do not support navigation.
  if (!web_state_->IsRealized()) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kNavigationTabNotRealized}));

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
  std::move(callback).Run(base::ok());
}

NavigateTool::NavigateTool(const std::string& url,
                           base::WeakPtr<web::WebState> web_state,
                           WebStateList* web_state_list,
                           UrlLoadingBrowserAgent* url_loader)
    : url_(url),
      web_state_(web_state),
      web_state_list_(web_state_list),
      url_loader_(url_loader) {}

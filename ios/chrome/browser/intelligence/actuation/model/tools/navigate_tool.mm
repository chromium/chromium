// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/tools/navigate_tool.h"

#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

// static
base::expected<std::unique_ptr<NavigateTool>, ActuationTool::ActuationError>
NavigateTool::Create(const optimization_guide::proto::NavigateAction& action,
                     ProfileIOS* profile) {
  if (!action.has_tab_id() || !action.has_url()) {
    return base::unexpected(
        ActuationError{ActuationErrorCode::kToolCreationFailed,
                       "NavigateAction proto is missing tab id or url."});
  }

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  BrowserAndIndex browser_and_index = FindBrowserAndIndex(
      web::WebStateID::FromSerializedValue(action.tab_id()),
      browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular));
  int tab_index = browser_and_index.tab_index;
  Browser* browser = browser_and_index.browser;
  if (tab_index == WebStateList::kInvalidIndex || !browser) {
    return base::unexpected(
        ActuationError{ActuationErrorCode::kToolCreationFailed,
                       "Target tab isn't in any Browser."});
  }

  return std::unique_ptr<NavigateTool>(
      new NavigateTool(action.url(), tab_index, browser->GetWebStateList(),
                       UrlLoadingBrowserAgent::FromBrowser(browser)));
}

// TODO(crbug.com/474383578): Limit what URLs can be navigated to using the
// ActuationService.
void NavigateTool::Execute(ActuationCallback callback) {
  if (!web_state_list_ || !url_loader_) {
    std::move(callback).Run(
        base::unexpected(ActuationError{ActuationErrorCode::kExecutionFailed,
                                        "Missing required dependencies."}));
    return;
  }

  GURL url(url_);
  if (!url.is_valid()) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kExecutionFailed, "Invalid URL"}));
    return;
  }

  // TODO(crbug.com/472291687): UrlLoadingBrowserAgent does not support
  // navigating a background tab, so activate the targeted tab before
  // navigating.
  web_state_list_->ActivateWebStateAt(tab_index_);
  UrlLoadParams params = UrlLoadParams::InCurrentTab(url);
  params.from_chrome = true;
  url_loader_->Load(params);
  std::move(callback).Run(base::ok());
}

NavigateTool::NavigateTool(const std::string& url,
                           int tab_index,
                           WebStateList* web_state_list,
                           UrlLoadingBrowserAgent* url_loader)
    : url_(url),
      tab_index_(tab_index),
      web_state_list_(web_state_list),
      url_loader_(url_loader) {}

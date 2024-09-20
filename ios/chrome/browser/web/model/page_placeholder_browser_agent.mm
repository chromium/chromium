// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/page_placeholder_browser_agent.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/web/public/web_state.h"

BROWSER_USER_DATA_KEY_IMPL(PagePlaceholderBrowserAgent)

PagePlaceholderBrowserAgent::PagePlaceholderBrowserAgent(Browser* browser)
    : browser_(browser) {
  // All the BrowserAgent are attached to the Browser during the creation,
  // the WebStateList must be empty at this point.
  DCHECK(browser_->GetWebStateList()->empty())
      << "PagePlaceholderBrowserAgent created for a Browser with a non-empty "
         "WebStateList.";

  ProfileIOS* profile = browser_->GetProfile();
  session_restoration_service_observation_.Observe(
      SessionRestorationServiceFactory::GetForProfile(profile));
}

PagePlaceholderBrowserAgent::~PagePlaceholderBrowserAgent() {
  browser_ = nullptr;
}

#pragma mark - Public

void PagePlaceholderBrowserAgent::ExpectNewForegroundTab() {
  expecting_foreground_tab_ = true;
}

void PagePlaceholderBrowserAgent::AddPagePlaceholder() {
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  if (web_state && expecting_foreground_tab_) {
    PagePlaceholderTabHelper::FromWebState(web_state)
        ->AddPlaceholderForNextNavigation();
  }
}

void PagePlaceholderBrowserAgent::CancelPagePlaceholder() {
  if (!expecting_foreground_tab_) {
    return;
  }

  // Now that the new tab has been displayed, return to normal. Rather than
  // keep a reference to the previous tab, just turn off preview mode for all
  // tabs (since doing so is a no-op for the tabs that don't have it set).
  expecting_foreground_tab_ = false;

  WebStateList* web_state_list = browser_->GetWebStateList();
  const int web_state_list_size = web_state_list->count();
  for (int index = 0; index < web_state_list_size; ++index) {
    web::WebState* web_state_at_index = web_state_list->GetWebStateAt(index);
    PagePlaceholderTabHelper::FromWebState(web_state_at_index)
        ->CancelPlaceholderForNextNavigation();
  }
}

#pragma mark - SessionRestorationObserver

void PagePlaceholderBrowserAgent::WillStartSessionRestoration(
    Browser* browser) {
  // Nothing to do.
}

void PagePlaceholderBrowserAgent::SessionRestorationFinished(
    Browser* browser,
    const std::vector<web::WebState*>& restored_web_states) {
  // Ignore the event if it does not correspond to the browser this
  // object is bound to (which can happen with the optimised session
  // storage code).
  if (browser_.get() != browser) {
    return;
  }

  // Setup the placeholder for the restored tabs if necessary.
  for (web::WebState* web_state : restored_web_states) {
    const GURL& visible_url = web_state->GetVisibleURL();
    if (visible_url.is_valid() && visible_url != kChromeUINewTabURL) {
      PagePlaceholderTabHelper::FromWebState(web_state)
          ->AddPlaceholderForNextNavigation();
    }
  }
}

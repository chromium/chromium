// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/page_placeholder_browser_agent.h"

#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/web/public/web_state.h"

BROWSER_USER_DATA_KEY_IMPL(PagePlaceholderBrowserAgent)

PagePlaceholderBrowserAgent::PagePlaceholderBrowserAgent(Browser* browser)
    : web_state_list_(browser->GetWebStateList()) {
  // All the BrowserAgent are attached to the Browser during the creation,
  // the WebStateList must be empty at this point.
  DCHECK(web_state_list_->empty())
      << "PagePlaceholderBrowserAgent created for a Browser with a non-empty "
         "WebStateList.";
}

PagePlaceholderBrowserAgent::~PagePlaceholderBrowserAgent() {}

#pragma mark - Public

void PagePlaceholderBrowserAgent::ExpectNewForegroundTab() {
  expecting_foreground_tab_ = true;
}

void PagePlaceholderBrowserAgent::AddPagePlaceholder() {
  web::WebState* web_state =
      web_state_list_ ? web_state_list_->GetActiveWebState() : nullptr;
  if (web_state && expecting_foreground_tab_) {
    PagePlaceholderTabHelper::FromWebState(web_state)
        ->AddPlaceholderForNextNavigation();
  }
}

void PagePlaceholderBrowserAgent::CancelPagePlaceholder() {
  if (expecting_foreground_tab_) {
    // Now that the new tab has been displayed, return to normal. Rather than
    // keep a reference to the previous tab, just turn off preview mode for all
    // tabs (since doing so is a no-op for the tabs that don't have it set).
    expecting_foreground_tab_ = false;

    int web_state_list_size = web_state_list_ ? web_state_list_->count() : 0;
    for (int index = 0; index < web_state_list_size; ++index) {
      web::WebState* web_state_at_index = web_state_list_->GetWebStateAt(index);
      PagePlaceholderTabHelper::FromWebState(web_state_at_index)
          ->CancelPlaceholderForNextNavigation();
    }
  }
}

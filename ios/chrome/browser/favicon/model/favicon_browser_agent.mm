// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/model/favicon_browser_agent.h"

#import "base/check.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

FaviconBrowserAgent::FaviconBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  StartObserving(browser);
}

FaviconBrowserAgent::~FaviconBrowserAgent() {
  StopObserving();
}

#pragma mark - TabsDependencyInstaller

void FaviconBrowserAgent::OnWebStateInserted(web::WebState* web_state) {
  CHECK(web_state->IsRealized());
  const GURL& visible_url = web_state->GetVisibleURL();
  if (visible_url.is_valid()) {
    // Starts fetching the favicon for the WebState.
    favicon::WebFaviconDriver::FromWebState(web_state)->FetchFavicon(
        visible_url, /*is_same_document=*/false);
  }
}

void FaviconBrowserAgent::OnWebStateRemoved(web::WebState* web_state) {
  // Nothing to do.
}

void FaviconBrowserAgent::OnWebStateDeleted(web::WebState* web_state) {
  // Nothing to do.
}

void FaviconBrowserAgent::OnActiveWebStateChanged(web::WebState* old_active,
                                                  web::WebState* new_active) {
  // Nothing to do.
}

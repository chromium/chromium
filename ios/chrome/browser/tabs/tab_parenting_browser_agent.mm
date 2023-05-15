// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_parenting_browser_agent.h"

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/tab_parenting_global_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(TabParentingBrowserAgent)

TabParentingBrowserAgent::TabParentingBrowserAgent(Browser* browser) {
  browser->AddObserver(this);
  browser->GetWebStateList()->AddObserver(this);
}

TabParentingBrowserAgent::~TabParentingBrowserAgent() = default;

// BrowserObserver
void TabParentingBrowserAgent::BrowserDestroyed(Browser* browser) {
  // Stop observing web state list.
  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
}

// WebStateListObserver
void TabParentingBrowserAgent::WebStateInsertedAt(WebStateList* web_state_list,
                                                  web::WebState* web_state,
                                                  int index,
                                                  bool activating) {
  TabParentingGlobalObserver::GetInstance()->OnTabParented(web_state);
}

void TabParentingBrowserAgent::WebStateReplacedAt(WebStateList* web_state_list,
                                                  web::WebState* old_web_state,
                                                  web::WebState* new_web_state,
                                                  int index) {
  TabParentingGlobalObserver::GetInstance()->OnTabParented(new_web_state);
}

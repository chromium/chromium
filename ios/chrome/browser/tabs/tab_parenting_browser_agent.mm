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

#pragma mark - BrowserObserver

void TabParentingBrowserAgent::BrowserDestroyed(Browser* browser) {
  // Stop observing web state list.
  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
}

#pragma mark - WebStateListObserver

void TabParentingBrowserAgent::WebStateListChanged(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateSelection& selection) {
  switch (change.type()) {
    case WebStateListChange::Type::kSelectionOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach:
      // Do nothing when a WebState is detached.
      break;
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      TabParentingGlobalObserver::GetInstance()->OnTabParented(
          replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      TabParentingGlobalObserver::GetInstance()->OnTabParented(
          insert_change.inserted_web_state());
      break;
    }
  }
}

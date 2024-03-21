// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tab_parenting_browser_agent.h"

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/tab_parenting_global_observer.h"

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

void TabParentingBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
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
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }
}

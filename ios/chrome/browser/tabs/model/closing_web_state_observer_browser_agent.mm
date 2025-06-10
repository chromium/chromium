// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/closing_web_state_observer_browser_agent.h"

#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

ClosingWebStateObserverBrowserAgent::~ClosingWebStateObserverBrowserAgent() =
    default;

ClosingWebStateObserverBrowserAgent::ClosingWebStateObserverBrowserAgent(
    Browser* browser)
    : BrowserUserData(browser) {
  DCHECK(!browser_->GetProfile()->IsOffTheRecord());
  web_state_list_observation_.Observe(browser_->GetWebStateList());
}

void ClosingWebStateObserverBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      web::WebState* detached_web_state = detach_change.detached_web_state();
      const GURL& url = detached_web_state->GetLastCommittedURL();
      UMA_HISTOGRAM_BOOLEAN("IOS.ClosedTabIsAboutBlank", url.IsAboutBlank());
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      // Do nothing whan a WebState is replaced.
      break;
    }
    case WebStateListChange::Type::kInsert:
      // Do nothing when a new WebState is inserted.
      break;
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

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/omnibox_geolocation_tab_helper.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OmniboxGeolocationTabHelper::OmniboxGeolocationTabHelper(
    web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

OmniboxGeolocationTabHelper::~OmniboxGeolocationTabHelper() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void OmniboxGeolocationTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK(web_state->GetNavigationManager());
  web::NavigationItem* navigation_item =
      web_state->GetNavigationManager()->GetPendingItem();

  if (!navigation_item) {
    // Pending item may not exist due to the bug in //ios/web layer.
    // TODO(crbug.com/899827): remove this early return once GetPendingItem()
    // always return valid object inside WebStateObserver::DidStartNavigation()
    // callback.
    //
    // Note that GetLastCommittedItem() returns null if navigation manager does
    // not have committed items (which is normal situation).
    return;
  }
}

void OmniboxGeolocationTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  [[OmniboxGeolocationController sharedInstance]
      finishPageLoadForWebState:web_state
                    loadSuccess:(load_completion_status ==
                                 web::PageLoadCompletionStatus::SUCCESS)];
}

void OmniboxGeolocationTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  web_state_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(OmniboxGeolocationTabHelper)

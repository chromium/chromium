// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_TAB_HELPER_H_

#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A tab helper which handles the current device location for omnibox search
// queries.
class OmniboxGeolocationTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<OmniboxGeolocationTabHelper> {
 public:
  // Not copyable or moveable
  OmniboxGeolocationTabHelper(const OmniboxGeolocationTabHelper&) = delete;
  OmniboxGeolocationTabHelper& operator=(const OmniboxGeolocationTabHelper&) =
      delete;
  ~OmniboxGeolocationTabHelper() override;

 private:
  friend class web::WebStateUserData<OmniboxGeolocationTabHelper>;

  explicit OmniboxGeolocationTabHelper(web::WebState* web_state);

  // web::WebStateObserver
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  web::WebState* web_state_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_GEOLOCATION_OMNIBOX_GEOLOCATION_TAB_HELPER_H_

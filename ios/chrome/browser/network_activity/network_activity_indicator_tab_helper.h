// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NETWORK_ACTIVITY_NETWORK_ACTIVITY_INDICATOR_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_NETWORK_ACTIVITY_NETWORK_ACTIVITY_INDICATOR_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// Handles listening to WebState network activity to control network activity
// indicator.
class NetworkActivityIndicatorTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<NetworkActivityIndicatorTabHelper> {
 public:
  ~NetworkActivityIndicatorTabHelper() override;

  static void CreateForWebState(web::WebState* web_state, NSString* tab_id);

 private:
  friend class web::WebStateUserData<NetworkActivityIndicatorTabHelper>;

  NetworkActivityIndicatorTabHelper(web::WebState* web_state, NSString* tab_id);

  // web::WebStateObserver overrides:
  void DidStartLoading(web::WebState* web_state) override;
  void DidStopLoading(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Clears any network activity state associated with this activity.
  void Stop();

  // Key used to uniquely identify this activity.
  NSString* network_activity_key_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(NetworkActivityIndicatorTabHelper);
};

#endif  // IOS_CHROME_BROWSER_NETWORK_ACTIVITY_NETWORK_ACTIVITY_INDICATOR_TAB_HELPER_H_

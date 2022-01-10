// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_TAB_HELPER_H_

#import "ios/web/public/web_state_user_data.h"

@protocol CaptivePortalTabHelperDelegate;

namespace web {
class WebState;
}

// Associates a Tab to a CaptivePortalDetector and manages its lifetime.
class CaptivePortalTabHelper
    : public web::WebStateUserData<CaptivePortalTabHelper> {
 public:
  CaptivePortalTabHelper(const CaptivePortalTabHelper&) = delete;
  CaptivePortalTabHelper& operator=(const CaptivePortalTabHelper&) = delete;

  ~CaptivePortalTabHelper() override;

  // Creates a Tab Helper and attaches it to |web_state|. The |delegate| is not
  // retained by the CaptivePortalTabHelper and must not be nil.
  static void CreateForWebState(web::WebState* web_state,
                                id<CaptivePortalTabHelperDelegate> delegate);

  // Displays the Captive Portal Login page at |landing_url|.
  void DisplayCaptivePortalLoginPage(GURL landing_url);

 private:
  friend class web::WebStateUserData<CaptivePortalTabHelper>;

  CaptivePortalTabHelper(id<CaptivePortalTabHelperDelegate> delegate);

  // The delegate to notify when the user performs an action in response to the
  // captive portal detector state.
  __weak id<CaptivePortalTabHelperDelegate> delegate_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_TAB_HELPER_H_

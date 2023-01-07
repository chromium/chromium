// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_TAB_HELPER_H_

#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/web/public/web_state_user_data.h"

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

  // Displays the Captive Portal Login page at `landing_url`.
  void DisplayCaptivePortalLoginPage(GURL landing_url);

  void SetTabInsertionBrowserAgent(TabInsertionBrowserAgent* insertionAgent);

 private:
  friend class web::WebStateUserData<CaptivePortalTabHelper>;

  CaptivePortalTabHelper(web::WebState* web_state);

  TabInsertionBrowserAgent* insertionAgent_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_TAB_HELPER_H_

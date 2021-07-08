// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_TAB_HELPER_H_

#include "base/macros.h"
#import "ios/chrome/browser/app_launcher/app_launcher_abuse_detector.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

class AppLauncherTabHelperDelegate;
@class AppLauncherAbuseDetector;
class GURL;

// A tab helper that handles requests to launch another application.
class AppLauncherTabHelper
    : public web::WebStatePolicyDecider,
      public web::WebStateUserData<AppLauncherTabHelper> {
 public:
  ~AppLauncherTabHelper() override;

  // Creates a tab helper for |web_state| that uses |abuse_detector| to make
  // navigation policy decisions.
  static void CreateForWebState(web::WebState* web_state,
                                AppLauncherAbuseDetector* abuse_detector =
                                    [[AppLauncherAbuseDetector alloc] init]);

  // Returns true, if the |url| has a scheme for an external application
  // (eg. twitter:// , calshow://).
  static bool IsAppUrl(const GURL& url);

  // Sets the delegate.
  void SetDelegate(AppLauncherTabHelperDelegate* delegate);

  // Requests to open the application with |url|.
  // The method checks if the application for |url| has been opened repeatedly
  // by the |source_page_url| page in a short time frame, in that case a prompt
  // will appear to the user with an option to block the application from
  // launching. Then the method also checks for user interaction and for schemes
  // that require special handling (eg. facetime, mailto) and may present the
  // user with a confirmation dialog to open the application.
  void RequestToLaunchApp(const GURL& url,
                          const GURL& source_page_url,
                          bool link_transition);

  // web::WebStatePolicyDecider implementation
  web::WebStatePolicyDecider::PolicyDecision ShouldAllowRequest(
      NSURLRequest* request,
      const web::WebStatePolicyDecider::RequestInfo& request_info) override;

 private:
  friend class web::WebStateUserData<AppLauncherTabHelper>;

  // Constructor for AppLauncherTabHelper. |abuse_detector| provides policy for
  // launching apps.
  AppLauncherTabHelper(web::WebState* web_state,
                       AppLauncherAbuseDetector* abuse_detector);

  // Getter for the delegate.
  AppLauncherTabHelperDelegate* delegate() const { return delegate_; }

  // The WebState that this object is attached to.
  web::WebState* web_state_ = nullptr;

  // Used to check for repeated launches and provide policy for launching apps.
  AppLauncherAbuseDetector* abuse_detector_ = nil;

  // Used to launch apps and present UI.
  AppLauncherTabHelperDelegate* delegate_ = nullptr;

  // Returns whether there is a prompt shown by |RequestToOpenUrl| or not.
  bool is_prompt_active_ = false;

  // Must be last member to ensure it is destroyed last.
  base::WeakPtrFactory<AppLauncherTabHelper> weak_factory_{this};

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(AppLauncherTabHelper);
};

#endif  // IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_TAB_HELPER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#include "base/macros.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

@protocol NewTabPageTabHelperDelegate;

// NewTabPageTabHelper which manages a single NTP per tab.
class NewTabPageTabHelper : public web::WebStateObserver,
                            public web::WebStateUserData<NewTabPageTabHelper> {
 public:
  ~NewTabPageTabHelper() override;

  static void CreateForWebState(web::WebState* web_state,
                                id<NewTabPageTabHelperDelegate> delegate);

  // Returns true when the current web_state is an NTP and the underlying
  // controllers have been created.
  bool IsActive() const;

  // Disables this tab helper.  This is useful when navigating away from an NTP,
  // so the tab helper can be disabled immediately, and before any potential
  // WebStateObserver callback.
  void Deactivate();

 private:
  NewTabPageTabHelper(web::WebState* web_state,
                      id<NewTabPageTabHelperDelegate> delegate);

  // web::WebStateObserver overrides:
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;

  // Enable or disable the tab helper.
  void SetActive(bool active);

  // Used to present and dismiss the NTP.
  __weak id<NewTabPageTabHelperDelegate> delegate_ = nil;

  // The WebState with which this object is associated.
  web::WebState* web_state_ = nullptr;

  // |YES| if the current tab helper is active.
  BOOL active_;

  DISALLOW_COPY_AND_ASSIGN(NewTabPageTabHelper);
};

#endif  // IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_H_

#import <UIKit/UIKit.h>
#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol NewTabPageTabHelperDelegate;

namespace web {
class NavigationItem;
}

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

  // Sometimes the underlying ios/web page used for the NTP (about://newtab)
  // takes a long time to load.  Loading any page before the newtab is committed
  // will leave ios/web in a bad state.  See: crbug.com/925304 for more context.
  // Remove this when ios/web supports queueing multiple loads during this
  // state.
  bool IgnoreLoadRequests() const;

 private:
  friend class web::WebStateUserData<NewTabPageTabHelper>;

  NewTabPageTabHelper(web::WebState* web_state,
                      id<NewTabPageTabHelperDelegate> delegate);

  // web::WebStateObserver overrides:
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;

  // Enable or disable the tab helper.
  void SetActive(bool active);

  // Sets the NTP's NavigationItem title and virtualURL to the appropriate
  // string and chrome://newtab respectively.
  void UpdateItem(web::NavigationItem* item);

  // Returns true if an |url| is either chrome://newtab or about://newtab.
  bool IsNTPURL(const GURL& url);

  // Sets the |ignore_load_requests_| flag to YES and starts the ignore load
  // timer.
  void EnableIgnoreLoadRequests();

  // Sets the |ignore_load_requests_| flag to NO and stops the ignore load
  // timer.
  void DisableIgnoreLoadRequests();

  // Used to present and dismiss the NTP.
  __weak id<NewTabPageTabHelperDelegate> delegate_ = nil;

  // The WebState with which this object is associated.
  web::WebState* web_state_ = nullptr;

  // |YES| if the current tab helper is active.
  BOOL active_;

  // |YES| if the NTP's underlying ios/web page is still loading.
  BOOL ignore_load_requests_ = NO;

  // Ensure the ignore_load_requests_ flag is never set to NO for more than
  // |kMaximumIgnoreLoadRequestsTime| seconds.
  std::unique_ptr<base::OneShotTimer> ignore_load_requests_timer_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(NewTabPageTabHelper);
};

#endif  // IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_H_

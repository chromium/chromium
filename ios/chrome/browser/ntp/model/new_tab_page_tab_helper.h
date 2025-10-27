// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_NEW_TAB_PAGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_NEW_TAB_PAGE_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ios/chrome/browser/discover_feed/model/feed_constants.h"
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
  NewTabPageTabHelper(const NewTabPageTabHelper&) = delete;
  NewTabPageTabHelper& operator=(const NewTabPageTabHelper&) = delete;

  ~NewTabPageTabHelper() override;

  // Sets the NTP's NavigationItem title and virtualURL to the appropriate
  // string and chrome://newtab respectively.
  static void UpdateItem(web::NavigationItem* item);

  // Sets the delegate. The delegate is not owned by the tab helper.
  void SetDelegate(id<NewTabPageTabHelperDelegate> delegate);

  // Setter/Getter for whether to show the Start Surface.
  bool ShouldShowStartSurface() const;
  void SetShowStartSurface(bool show_start_surface);

  // Saves the NTP scroll position for when users navigate back to it.
  void SetNTPScrollPosition(CGFloat scroll_position);

  // Returns the saved scroll position of the associated NTP.
  CGFloat GetNTPScrollPosition();

  // web::WebStateObserver overrides:
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;

 private:
  // For access to IsActive(...) method.
  friend bool IsVisibleURLNewTabPage(web::WebState*);
  friend class web::WebStateUserData<NewTabPageTabHelper>;

  explicit NewTabPageTabHelper(web::WebState* web_state);

  // Enable or disable the tab helper.
  void SetActive(bool active);

  // Returns true when the current web_state is an NTP and the underlying
  // controllers have been created. Should not be accessed directly instead
  // use the helper function IsVisibleURLNewTabPage(...) which handles the
  // case where the WebState is unrealized and the tab helper not created.
  bool IsActive() const;

  // Used to present and dismiss the NTP.
  __weak id<NewTabPageTabHelperDelegate> delegate_ = nil;

  // The WebState with which this object is associated.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // `true` if the current tab helper is active.
  bool active_ = false;

  // `YES` if the NTP for this WebState should be configured to show the Start
  // Surface.
  BOOL show_start_surface_ = false;

  // The saved scroll position of the associated NTP. We don't know what the top
  // position of the NTP is in advance, so we use `-CGFLOAT_MAX` to indicate
  // that it's scrolled to top.
  CGFloat scroll_position_ = -CGFLOAT_MAX;
};

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_NEW_TAB_PAGE_TAB_HELPER_H_

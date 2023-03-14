// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#include "base/timer/timer.h"
#include "ios/chrome/browser/discover_feed/feed_constants.h"
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

  // Returns the default selected feed for the NTP.
  static FeedType DefaultFeedType();

  // Sets the delegate. The delegate is not owned by the tab helper.
  void SetDelegate(id<NewTabPageTabHelperDelegate> delegate);

  // Setter/Getter for whether to show the Start Surface.
  bool ShouldShowStartSurface() const;
  void SetShowStartSurface(bool show_start_surface);

  // Returns true when the current web_state is an NTP and the underlying
  // controllers have been created.
  bool IsActive() const;

  // Returns true if an `url` is either chrome://newtab or about://newtab.
  bool IsNTPURL(const GURL& url);

  // Returns the selected feed for the next NTP.
  FeedType GetNextNTPFeedType();

  // Sets the default feed for the next NTP.
  void SetNextNTPFeedType(FeedType feed_type);

  // Returns whether the next NTP should be initially scrolled to the feed.
  bool GetNextNTPScrolledToFeed();

  // Sets whether the next NTP should be initially scrolled to the feed.
  void SetNextNTPScrolledToFeed(bool scrolled_to_feed);

  // Saves the NTP state for when users navigate back to it.
  void SaveNTPState(CGFloat scroll_position, FeedType feed_type);

  // Returns the saved scroll position of the NTP from `SaveNTPState`.
  CGFloat ScrollPositionFromSavedState();

 private:
  friend class web::WebStateUserData<NewTabPageTabHelper>;

  explicit NewTabPageTabHelper(web::WebState* web_state);

  // web::WebStateObserver overrides:
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;

  // Enable or disable the tab helper.
  void SetActive(bool active);

  // Used to present and dismiss the NTP.
  __weak id<NewTabPageTabHelperDelegate> delegate_ = nil;

  // The WebState with which this object is associated.
  web::WebState* web_state_ = nullptr;

  // `true` if the current tab helper is active.
  bool active_ = false;

  // `YES` if the NTP for this WebState should be configured to show the Start
  // Surface.
  BOOL show_start_surface_ = false;

  // The default feed type of the next NTP.
  FeedType next_ntp_feed_type_;

  // Whether the next NTP should be initially scrolled to the feed.
  BOOL next_ntp_scrolled_to_feed_ = NO;

  // The saved scroll position for navigating back to the NTP.
  CGFloat saved_scroll_position_ = -CGFLOAT_MAX;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_TAB_HELPER_H_

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_FOLLOW_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_FOLLOW_FOLLOW_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

@class FollowWebPageURLs;
@protocol FollowIPHPresenter;
@protocol FollowMenuUpdater;

// FollowTabHelper encapsulates tab behavior related to following channels.
class FollowTabHelper : public web::WebStateObserver,
                        public web::WebStateUserData<FollowTabHelper> {
 public:
  FollowTabHelper(const FollowTabHelper&) = delete;
  FollowTabHelper& operator=(const FollowTabHelper&) = delete;

  ~FollowTabHelper() override;

  // Creates the TabHelper and attaches to |web_state|. |web_state| must not be
  // null.
  static void CreateForWebState(web::WebState* web_state);

  // Sets the presenter for follow in-product help (IPH). |presenter| is not
  // retained by this tab helper.
  void set_follow_iph_presenter(id<FollowIPHPresenter> presenter) {
    follow_iph_presenter_ = presenter;
  }

  // Sets the value of shoud_update_follow_item_.
  void set_should_update_follow_item(bool shoud_update_follow_item) {
    should_update_follow_item_ = shoud_update_follow_item;
  }

  // Sets the follow meue updater. |follow_menu_updater| is not retained by this
  // tab helper.
  void set_follow_menu_updater(id<FollowMenuUpdater> follow_menu_updater);

  // Removes the follow meue updater.
  void remove_follow_menu_updater();

 private:
  friend class web::WebStateUserData<FollowTabHelper>;

  explicit FollowTabHelper(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidRedirectNavigation(
      web::WebState* web_state,
      web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Helper functions.
  void UpdateFollowMenuItem(FollowWebPageURLs* web_page_urls);

  web::WebState* web_state_ = nullptr;

  // Presenter for follow in-product help (IPH).
  __weak id<FollowIPHPresenter> follow_iph_presenter_ = nil;

  // Manages this object as an observer of |web_state_|.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // True if the follow menu item should be updated. Ex. Set to true when a new
  // navigation starts, to ensure the follow menu item would be updated when the
  // page finishes loading.
  bool should_update_follow_item_ = false;

  // Used to update the follow menu item.
  __weak id<FollowMenuUpdater> follow_menu_updater_ = nil;

  base::WeakPtrFactory<FollowTabHelper> weak_ptr_factory_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_FOLLOW_TAB_HELPER_H_

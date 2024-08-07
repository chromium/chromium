// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"

namespace history {
struct DailyVisitsResult;
}

namespace web {
class WebState;
}

class GURL;
@class WebPageURLs;
@protocol FollowMenuUpdater;
@protocol HelpCommands;

// FollowTabHelper encapsulates tab behavior related to following channels.
class FollowTabHelper : public web::WebStateObserver,
                        public web::WebStateUserData<FollowTabHelper> {
 public:
  FollowTabHelper(const FollowTabHelper&) = delete;
  FollowTabHelper& operator=(const FollowTabHelper&) = delete;

  ~FollowTabHelper() override;

  // Sets the handler for showing follow in-product help (IPH). `help_handler`
  // is not retained by this tab helper.
  void set_help_handler(id<HelpCommands> help_handler);

  // Sets the value of `shoud_update_follow_item_`.
  void set_should_update_follow_item(bool shoud_update_follow_item) {
    should_update_follow_item_ = shoud_update_follow_item;
  }

  // Sets the follow menu updater. `follow_menu_updater` is not retained by this
  // tab helper.
  void SetFollowMenuUpdater(id<FollowMenuUpdater> follow_menu_updater);

  // Removes the follow menu updater.
  void RemoveFollowMenuUpdater();

  // Updates the follow menu item.
  void UpdateFollowMenuItem();

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

  // Invoked asynchronously after fetching the `web_page_urls` from the
  // successfully loaded page at `url`. The initial time of the page load
  // is recorded in `page_load_time` and can be used to limit the delay
  // before displaying UI.
  void OnSuccessfulPageLoad(const GURL& url,
                            base::Time page_load_time,
                            WebPageURLs* web_page_urls);

  // Invoked with daily visit from history `result`. The initial time of the
  // page load is recorded in `page_load_time` and can be used to limit the
  // delay before displaying UI. `recommended_url` is the recommended URL for
  // the website whose visit have been queried.
  void OnDailyVisitQueryResult(base::Time page_load_time,
                               NSURL* recommended_url,
                               history::DailyVisitsResult result);

  // Updates follow menu item. `web_page_urls` is the page url object used to
  // check follow status.
  void UpdateFollowMenuItemWithURL(WebPageURLs* web_page_urls);

  // Presents the Follow in-product help (IPH) for `recommended_url`.
  void PresentFollowIPH(NSURL* recommended_url);

  raw_ptr<web::WebState> web_state_ = nullptr;

  // Presenter for follow in-product help (IPH).
  __weak id<HelpCommands> help_handler_ = nil;

  // Manages this object as an observer of `web_state_`.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // True if the follow menu item should be updated. Ex. Set to true when a new
  // navigation starts, to ensure the follow menu item would be updated when the
  // page finishes loading.
  bool should_update_follow_item_ = false;

  // Used to update the follow menu item.
  __weak id<FollowMenuUpdater> follow_menu_updater_ = nil;

  base::CancelableTaskTracker history_task_tracker_;
  base::WeakPtrFactory<FollowTabHelper> weak_ptr_factory_{this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_TAB_HELPER_H_

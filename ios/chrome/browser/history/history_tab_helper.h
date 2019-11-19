// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_

#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/history_types.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace history {
class HistoryService;
}  // namespace history

namespace web {
class NavigationItem;
}  // namespace web

// HistoryTabHelper updates the history database based on navigation events from
// its parent WebState.
class HistoryTabHelper : public history::Context,
                         public web::WebStateObserver,
                         public web::WebStateUserData<HistoryTabHelper> {
 public:
  ~HistoryTabHelper() override;

  // Updates history with the specified navigation.
  void UpdateHistoryForNavigation(
      const history::HistoryAddPageArgs& add_page_args);

  // Sends the page title to the history service.
  void UpdateHistoryPageTitle(const web::NavigationItem& item);

  // Sets whether the navigation should be send to the HistoryService or saved
  // for later (this will generally be set to true while the WebState is used
  // for pre-rendering).
  void SetDelayHistoryServiceNotification(bool delay_notification);

 private:
  friend class web::WebStateUserData<HistoryTabHelper>;

  // Constructs a new HistoryTabHelper.
  explicit HistoryTabHelper(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void TitleWasSet(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Helper function to return the history service. May return null.
  history::HistoryService* GetHistoryService();

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  // Hold navigation entries that need to be added to the history database.
  // Pre-rendered WebStates do not write navigation data to the history DB
  // immediately, instead they are cached in this vector and added when it
  // is converted to a non-pre-rendered state.
  std::vector<history::HistoryAddPageArgs> recorded_navigations_;

  // Controls whether the navigation will be sent to the HistoryService when
  // they happen or delayed. If delayed, then they will be sent when the flag
  // is set to false.
  bool delay_notification_ = false;

  // Number of title changes since the loading of the navigation started.
  int num_title_changes_;

  // The time that the current page finished loading. Only title changes within
  // a certain time period after the page load is complete will be saved to the
  // history system. Only applies to the main frame of the page.
  base::TimeTicks last_load_completion_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(HistoryTabHelper);
};

#endif  // IOS_CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_

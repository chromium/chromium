// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_TAB_HELPER_H_

#include <vector>

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/history_types.h"
#include "components/translate/core/browser/translate_driver.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace history {
class HistoryService;
}  // namespace history

namespace web {
class NavigationItem;
class NavigationContext;
}  // namespace web

// HistoryTabHelper updates the history database based on navigation events from
// its parent WebState.
class HistoryTabHelper
    : public history::Context,
      public web::WebStateObserver,
      public translate::TranslateDriver::LanguageDetectionObserver,
      public web::WebStateUserData<HistoryTabHelper> {
 public:
  HistoryTabHelper(const HistoryTabHelper&) = delete;
  HistoryTabHelper& operator=(const HistoryTabHelper&) = delete;

  ~HistoryTabHelper() override;

  // Updates history with the specified navigation. This is called by
  // DidFinishNavigation to update history state.
  void UpdateHistoryForNavigation(
      const history::HistoryAddPageArgs& add_page_args);

  // Sends the page title to the history service. Public for testing.
  void UpdateHistoryPageTitle(const web::NavigationItem& item);

  // Returns the history::HistoryAddPageArgs to use for adding a page to
  // history. Public for testing.
  history::HistoryAddPageArgs CreateHistoryAddPageArgs(
      web::NavigationItem* last_committed_item,
      web::NavigationContext* navigation_context);

  // Sets whether the navigation should be send to the HistoryService or saved
  // for later (this will generally be set to true while the WebState is used
  // for pre-rendering).
  void SetDelayHistoryServiceNotification(bool delay_notification);

  // TranslateDriver::LanguageDetectionObserver implementation.
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;

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
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Observes LanguageDetectionObserver, which notifies us when the language of
  // the contents of the current page has been determined.
  base::ScopedObservation<translate::TranslateDriver,
                          translate::TranslateDriver::LanguageDetectionObserver>
      translate_observation_{this};

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

  // Some cached state about the current navigation, used to identify it again
  // once a new navigation has happened.
  struct NavigationState {
    int nav_entry_id;
    GURL url;
  };
  std::optional<NavigationState> cached_navigation_state_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_HISTORY_MODEL_HISTORY_TAB_HELPER_H_

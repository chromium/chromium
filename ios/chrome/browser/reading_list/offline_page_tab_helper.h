// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_OFFLINE_PAGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_READING_LIST_OFFLINE_PAGE_TAB_HELPER_H_

#include <memory>
#include <string>

#include "components/reading_list/core/reading_list_model_observer.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace base {
class RepeatingTimer;
}
namespace web {
class NavigationContext;
class WebState;
}  // namespace web

class ReadingListModel;

// Responsible for presenting distilled version of page in the attached
// WebState. This TabHelper will replace the DOM of the WKWebView with the
// distilled version stored on disk. It will also update the URL of the
// navigation back to the online URL to make sure online page is loaded on
// reload. The distilled page will be displayed if:
//   - The load is slow (possibly due to poor network conditions).
//   - The load has failed (possibly because the device does not have internet).
//     It is possible to trigger a page load failure to load the distilled
//     version of |url| by loading chrome://offline?entryURL=url.
class OfflinePageTabHelper : public web::WebStateUserData<OfflinePageTabHelper>,
                             public web::WebStateObserver,
                             ReadingListModelObserver {
 public:
  // Creates TabHelper. |web_state| and |model| must not be null.
  static void CreateForWebState(web::WebState* web_state,
                                ReadingListModel* model);
  ~OfflinePageTabHelper() override;

  // Returns true if distilled version is currently being presented.
  bool presenting_offline_page() const { return presenting_offline_page_; }

  // Returns true if reading list model has processed entry for the given url.
  bool HasDistilledVersionForOnlineUrl(const GURL& url) const;

 private:
  friend class web::WebStateUserData<OfflinePageTabHelper>;

  OfflinePageTabHelper(web::WebState* web_state, ReadingListModel* model);

  // Detach from both WebState and ReadingListModel if any of these two becomes
  // unavailable.
  void Detach();

  // WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // ReadingListModelObserver:
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override;

  // Presents distilled version of the page if reading list model has processed
  // entry for the given url.
  // Note: This method will replace the last committed navigation item. If the
  // pending navigation was not yet committed, it returns immediately and is
  // noop. In these conditions, the offline page will be presented on navigation
  // commit.
  // TODO(crbug.com/936773): handle uncommitted navigations.
  void PresentOfflinePageForOnlineUrl(const GURL& url);

  // Starts repeating |timer_| which will fire |CheckLoadingProgress| method.
  void StartCheckingLoadingProgress(const GURL& url);
  // Stops repeating |timer_|.
  void StopCheckingLoadingProgress();
  // Tracks the page loading progress and presents distilled version of the page
  // if the following conditions are met:
  //  - the page load is slow
  //  - the loading progress did not reach 75%
  //  - reading list model has processed entry for the given url
  void CheckLoadingProgress(const GURL& url);
  // Loads |data| into the web_state() if |offline_navigation| is equal to
  // |last_navigation_started_|. |extension| is used to determine the MIMEType
  // of |data|.
  void LoadData(int offline_navigation,
                const GURL& url,
                const std::string& extension,
                const std::string& data);

  // Returns the URL of the Reading List entry given a navigation URL.
  GURL GetOnlineURLFromNavigationURL(const GURL& url) const;

  web::WebState* web_state_ = nullptr;
  ReadingListModel* reading_list_model_ = nullptr;

  // The initial URL of the navigation. This URL will not follow the
  // redirections so it may be different from GetLastCommittedURL.
  GURL initial_navigation_url_;
  // true if distilled version is currently being displayed.
  bool presenting_offline_page_ = false;
  // Timer started with navigation. When this timer fires, this tab helper may
  // attempt to present distilled version of the page.
  std::unique_ptr<base::RepeatingTimer> timer_;
  // Number of times when |timer_| fired. Used to determine if the load progress
  // is slow and it's actually time to present distilled version of the page.
  int try_number_ = 0;
  // A counter of the navigation sterted in web_state. Is used as an ID for the
  // navigation that triggered an offline page presentation.
  long last_navigation_started_ = 0;
  // The URL of the offline page the is presented by this TabHelper.
  // Is used to ignore the navigation triggered by WebState::LoadData.
  GURL offline_navigation_triggered_;
  // Whether the page could not be loaded, either because it was too slow, or
  // because an error occurred.
  bool loading_slow_or_failed_ = false;
  // Whether the navigation for which this tab helper started the timer has been
  // committed.
  bool navigation_committed_ = false;
  // Whether the latest navigation started for this tab helper was initiated
  // with chrome://offline URL.
  bool is_offline_navigation_ = false;
  // Some parameters of the latest navigation started observed by this tab
  // helper.
  ui::PageTransition navigation_transition_type_ = ui::PAGE_TRANSITION_FIRST;
  bool navigation_is_renderer_initiated_ = false;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_OFFLINE_PAGE_TAB_HELPER_H_

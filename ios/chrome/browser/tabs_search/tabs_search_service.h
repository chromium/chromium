// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_SEARCH_TABS_SEARCH_SERVICE_H_
#define IOS_CHROME_BROWSER_TABS_SEARCH_TABS_SEARCH_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/session_id.h"
#import "ios/chrome/browser/ui/history/ios_browsing_history_driver.h"
#import "ios/chrome/browser/ui/history/ios_browsing_history_driver_delegate.h"

class Browser;
class ChromeBrowserState;

namespace sessions {
class SerializedNavigationEntry;
}  // namespace sessions

namespace synced_sessions {
struct DistantTabsSet;
class SyncedSessions;
}  // namespace synced_sessions

namespace web {
class WebState;
}  // namespace web

// Service which provides search functionality across currently open application
// tabs.
class TabsSearchService : public IOSBrowsingHistoryDriverDelegate,
                          public KeyedService {
 public:
  TabsSearchService(ChromeBrowserState* browser_state);
  ~TabsSearchService() override;

  // A container to store matched WebStates with a reference to their associated
  // `browser`.
  struct TabsSearchBrowserResults {
    TabsSearchBrowserResults(const Browser*, const std::vector<web::WebState*>);
    ~TabsSearchBrowserResults();

    TabsSearchBrowserResults(const TabsSearchBrowserResults&);

    const Browser* browser;
    const std::vector<web::WebState*> web_states;
  };
  // Searches through tabs in all the Browsers associated with `browser_state`
  // for WebStates with current titles or URLs matching `term`. The matching
  // WebStates are returned to the `completion` callback in instances of
  // TabsSearchBrowserResults along with their associated Browser.
  void Search(const std::u16string& term,
              base::OnceCallback<void(std::vector<TabsSearchBrowserResults>)>
                  completion);

  // A pair representing a recently closed item. The `SessionID` can be used to
  // restore the item and is safe to store without lifetime concerns. The
  // `SerializedNavigationEntry` describes the visible navigation in order to
  // present the results to the user.
  typedef std::pair<SessionID, const sessions::SerializedNavigationEntry>
      RecentlyClosedItemPair;
  // Searches through recently closed tabs within `browser_state` in the same
  // manner as `Search`. Can't be called on an off the record `browser_state`.
  void SearchRecentlyClosed(
      const std::u16string& term,
      base::OnceCallback<void(std::vector<RecentlyClosedItemPair>)> completion);

  // Searches through Remote Tabs for tabs matching `term`. The matching tabs
  // returned in the vector are owned by the SyncedSessions instance passed to
  // the callback. Can't be called on an off the record `browser_state`.
  void SearchRemoteTabs(
      const std::u16string& term,
      base::OnceCallback<void(std::unique_ptr<synced_sessions::SyncedSessions>,
                              std::vector<synced_sessions::DistantTabsSet>)>
          completion);

  // Searches through synced history for the count of history results matching
  // term.
  // `completion` will be called with the result unless a new call to
  // SearchHistory is made. Only the last call to `SearchHistory` will continue
  // to be processed. Completion callbacks to earlier calls will not be run.
  // Can't be called on an off the record `browser_state`.
  void SearchHistory(const std::u16string& term,
                     base::OnceCallback<void(size_t result_count)> completion);

  // KeyedService implementation.
  void Shutdown() override;

  TabsSearchService(const TabsSearchService&) = delete;
  TabsSearchService& operator=(const TabsSearchService&) = delete;

 private:
  // Performs a search for `term` within `browsers`, returning the matching
  // WebStates and associated Browser to `completion`. Results are passed back
  // in instances of TabsSearchBrowserResults.
  void SearchWithinBrowsers(
      const std::set<Browser*>& browsers,
      const std::u16string& term,
      base::OnceCallback<void(std::vector<TabsSearchBrowserResults>)>
          completion);

  // IOSBrowsingHistoryDriverDelegate
  void HistoryQueryCompleted(
      const std::vector<history::BrowsingHistoryService::HistoryEntry>& results,
      const history::BrowsingHistoryService::QueryResultsInfo&
          query_results_info,
      base::OnceClosure continuation_closure) override;

  void HistoryWasDeleted() override {}

  void ShowNoticeAboutOtherFormsOfBrowsingHistory(
      BOOL should_show_notice) override {}

  // The associated BrowserState.
  ChromeBrowserState* browser_state_;
  // The most recent search history term.
  std::u16string ongoing_history_search_term_;
  // A callback to return history search results once the current in progress
  // history search completes. Will be null if no search is in progress.
  base::OnceCallback<void(size_t result_count)> history_search_callback_;
  // A history service instance for the associated `browser_state_`.
  std::unique_ptr<history::BrowsingHistoryService> history_service_;
  // Provides dependencies and funnels callbacks from BrowsingHistoryService.
  std::unique_ptr<IOSBrowsingHistoryDriver> history_driver_;
};

#endif  // IOS_CHROME_BROWSER_TABS_SEARCH_TABS_SEARCH_SERVICE_H_

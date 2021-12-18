// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_SEARCH_TABS_SEARCH_SERVICE_H_
#define IOS_CHROME_BROWSER_TABS_SEARCH_TABS_SEARCH_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/ui/history/ios_browsing_history_driver.h"
#import "ios/chrome/browser/ui/history/ios_browsing_history_driver_delegate.h"

class Browser;
class BrowserList;
class ChromeBrowserState;

namespace synced_sessions {
struct DistantTab;
}  // namespace synced_sessions

namespace web {
class WebState;
}  // namespace web

// Service which provides search functionality across currently open application
// tabs.
class TabsSearchService : public IOSBrowsingHistoryDriverDelegate,
                          public KeyedService {
 public:
  TabsSearchService(ChromeBrowserState* browser_state,
                    BrowserList* browser_list);
  ~TabsSearchService() override;

  // Searches through all the regular tabs in Browsers within |browser_list| and
  // provides the WebStates which match |term| to |completion|. |term| will be
  // matched against WebState's current title and URL.
  void Search(const std::u16string& term,
              base::OnceCallback<void(std::vector<web::WebState*>)> completion);

  // Performs a search through all the incognito tabs in Browsers within
  // |browser_list| in the same manner as |Search|.
  void SearchIncognito(
      const std::u16string& term,
      base::OnceCallback<void(std::vector<web::WebState*>)> completion);

  // Searches through Remote Tabs for tabs matching |term|.
  void SearchRemoteTabs(
      const std::u16string& term,
      base::OnceCallback<void(std::vector<synced_sessions::DistantTab*>)>
          completion);

  // Searches through synced history for the count of history results matching
  // term.
  // |completion| will be called with the result unless a new call to
  // SearchHistory is made. Only the last call to |SearchHistory| will continue
  // to be processed. Completion callbacks to earlier calls will not be run.
  void SearchHistory(const std::u16string& term,
                     base::OnceCallback<void(size_t result_count)> completion);

  TabsSearchService(const TabsSearchService&) = delete;
  TabsSearchService& operator=(const TabsSearchService&) = delete;

 private:
  // Performs a search for |term| within |browsers|, returning the matching
  // WebStates to |completion|.
  void SearchWithinBrowsers(
      const std::set<Browser*>& browsers,
      const std::u16string& term,
      base::OnceCallback<void(std::vector<web::WebState*>)> completion);

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
  // The list of Browsers to search through.
  BrowserList* browser_list_;
  // The most recent search history term.
  std::u16string ongoing_history_search_term_;
  // A callback to return history search results once the current in progress
  // history search completes. Will be null if no search is in progress.
  base::OnceCallback<void(size_t result_count)> history_search_callback_;
  // A history service instance for the current in progress history search.
  // This service be null when no history search is in progress.
  std::unique_ptr<history::BrowsingHistoryService> history_service_;
  // Provides dependencies and funnels callbacks from BrowsingHistoryService.
  std::unique_ptr<IOSBrowsingHistoryDriver> history_driver_;
};

#endif  // IOS_CHROME_BROWSER_TABS_SEARCH_TABS_SEARCH_SERVICE_H_

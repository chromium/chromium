// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_H_
#define IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/session_id.h"
#include "ios/chrome/browser/history/ui_bundled/ios_browsing_history_driver.h"
#include "ios/chrome/browser/history/ui_bundled/ios_browsing_history_driver_delegate.h"

class Browser;
class BrowserList;
class TabGroup;

namespace history {
class HistoryService;
class WebHistoryService;
}  // namespace history

namespace sessions {
class TabRestoreService;
class SerializedNavigationEntry;
}  // namespace sessions

namespace signin {
class IdentityManager;
}  // namespace signin

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

namespace synced_sessions {
struct DistantTabsSet;
class SyncedSessions;
}  // namespace synced_sessions

namespace syncer {
class SyncService;
}  // namespace syncer

namespace web {
class WebState;
}  // namespace web

// Service which provides search functionality across currently open application
// tabs.
class TabsSearchService : public IOSBrowsingHistoryDriverDelegate,
                          public KeyedService {
 public:
  using WebHistoryServiceGetter =
      base::RepeatingCallback<history::WebHistoryService*()>;

  TabsSearchService(bool is_off_the_record,
                    BrowserList* browser_list,
                    signin::IdentityManager* identity_manager,
                    syncer::SyncService* sync_service,
                    sessions::TabRestoreService* restore_service,
                    sync_sessions::SessionSyncService* session_sync_service,
                    history::HistoryService* history_service,
                    WebHistoryServiceGetter web_history_service_getter);
  ~TabsSearchService() override;

  // A container to store matched WebStates and TabGroups with a reference to
  // their associated `browser`.
  // `browser`.
  struct TabsSearchBrowserResults {
    TabsSearchBrowserResults(Browser*,
                             const std::vector<web::WebState*>,
                             const std::vector<const TabGroup*>);
    ~TabsSearchBrowserResults();

    TabsSearchBrowserResults(const TabsSearchBrowserResults&);

    raw_ptr<Browser> browser;
    const std::vector<web::WebState*> web_states;
    const std::vector<const TabGroup*> tab_groups;
  };
  // Searches through tabs in all the Browsers associated with `profile`
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
  // Searches through recently closed tabs within `profile` in the same
  // manner as `Search`. Can't be called on an off the record `profile`.
  void SearchRecentlyClosed(
      const std::u16string& term,
      base::OnceCallback<void(std::vector<RecentlyClosedItemPair>)> completion);

  // Searches through Remote Tabs for tabs matching `term`. The matching tabs
  // returned in the vector are owned by the SyncedSessions instance passed to
  // the callback. Can't be called on an off the record `profile`.
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
  // Can't be called on an off the record `profile`.
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

  // Is the service used for off-the-record BrowserState?
  const bool is_off_the_record_;
  // The KeyedServices used.
  raw_ptr<BrowserList> browser_list_;
  // The optional KeyedServices used (may be null when off-the-record).
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<syncer::SyncService> sync_service_;
  raw_ptr<sessions::TabRestoreService> restore_service_;
  raw_ptr<sync_sessions::SessionSyncService> session_sync_service_;
  raw_ptr<history::HistoryService> history_service_;
  WebHistoryServiceGetter web_history_service_getter_;

  // The most recent search history term.
  std::u16string ongoing_history_search_term_;
  // A callback to return history search results once the current in progress
  // history search completes. Will be null if no search is in progress.
  base::OnceCallback<void(size_t result_count)> history_search_callback_;
  // A history service instance for the associated `profile_`.
  std::unique_ptr<history::BrowsingHistoryService> browsing_history_service_;
  // Provides dependencies and funnels callbacks from BrowsingHistoryService.
  std::unique_ptr<IOSBrowsingHistoryDriver> history_driver_;
};

#endif  // IOS_CHROME_BROWSER_TABS_SEARCH_MODEL_TABS_SEARCH_SERVICE_H_

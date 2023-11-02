// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_IOS_BROWSING_HISTORY_DRIVER_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_IOS_BROWSING_HISTORY_DRIVER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/history/core/browser/browsing_history_driver.h"
#include "components/history/core/browser/browsing_history_service.h"
#include "url/gurl.h"

class ChromeBrowserState;
class IOSBrowsingHistoryDriverDelegate;

// A simple implementation of BrowsingHistoryServiceHandler that delegates to
// IOSBrowsingHistoryDriverDelegate for most actions.
class IOSBrowsingHistoryDriver : public history::BrowsingHistoryDriver {
 public:
  IOSBrowsingHistoryDriver(ChromeBrowserState* browser_state,
                           IOSBrowsingHistoryDriverDelegate* delegate);

  IOSBrowsingHistoryDriver(const IOSBrowsingHistoryDriver&) = delete;
  IOSBrowsingHistoryDriver& operator=(const IOSBrowsingHistoryDriver&) = delete;

  ~IOSBrowsingHistoryDriver() override;

 private:
  // history::BrowsingHistoryDriver implementation.
  void OnQueryComplete(
      const std::vector<history::BrowsingHistoryService::HistoryEntry>& results,
      const history::BrowsingHistoryService::QueryResultsInfo&
          query_results_info,
      base::OnceClosure continuation_closure) override;
  void OnRemoveVisitsComplete() override;
  void OnRemoveVisitsFailed() override;
  void OnRemoveVisits(
      const std::vector<history::ExpireHistoryArgs>& expire_list) override;
  void HistoryDeleted() override;
  void HasOtherFormsOfBrowsingHistory(bool has_other_forms,
                                      bool has_synced_results) override;
  bool AllowHistoryDeletions() override;
  bool ShouldHideWebHistoryUrl(const GURL& url) override;
  history::WebHistoryService* GetWebHistoryService() override;
  void ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      const syncer::SyncService* sync_service,
      history::WebHistoryService* history_service,
      base::OnceCallback<void(bool)> callback) override;

  // The current browser state.
  ChromeBrowserState* browser_state_;  // weak

  IOSBrowsingHistoryDriverDelegate* delegate_ = nullptr;  // weak
};

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_IOS_BROWSING_HISTORY_DRIVER_H_

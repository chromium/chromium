// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_IOS_BROWSING_HISTORY_DRIVER_DELEGATE_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_IOS_BROWSING_HISTORY_DRIVER_DELEGATE_H_

#include <vector>

#include "components/history/core/browser/browsing_history_service.h"

class IOSBrowsingHistoryDriverDelegate {
 public:
  IOSBrowsingHistoryDriverDelegate() = default;

  IOSBrowsingHistoryDriverDelegate(const IOSBrowsingHistoryDriverDelegate&) =
      delete;
  IOSBrowsingHistoryDriverDelegate& operator=(
      const IOSBrowsingHistoryDriverDelegate&) = delete;

  virtual void HistoryQueryCompleted(
      const std::vector<history::BrowsingHistoryService::HistoryEntry>& results,
      const history::BrowsingHistoryService::QueryResultsInfo&
          query_results_info,
      base::OnceClosure continuation_closure) = 0;

  virtual void HistoryWasDeleted() = 0;

  virtual void ShowNoticeAboutOtherFormsOfBrowsingHistory(
      BOOL should_show_notice) = 0;

 protected:
  virtual ~IOSBrowsingHistoryDriverDelegate() = default;
};

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_IOS_BROWSING_HISTORY_DRIVER_DELEGATE_H_

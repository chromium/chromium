// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_TABS_COUNTER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_TABS_COUNTER_H_

#import <map>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/browsing_data/core/counters/browsing_data_counter.h"
#import "ios/chrome/browser/browsing_data/model/tabs_closure_util.h"

class BrowserList;
class SessionRestorationService;

// TabsCounter is a BrowsingDataCounter used to compute the number of tabs that
// have a last navigation timestamp within the deletion time range.
class TabsCounter : public browsing_data::BrowsingDataCounter {
 public:
  class TabsResult : public FinishedResult {
   public:
    TabsResult(const TabsCounter* source,
               ResultInt tab_count,
               ResultInt window_count);

    TabsResult(const TabsResult&) = delete;
    TabsResult& operator=(const TabsResult&) = delete;

    ~TabsResult() override;

    int window_count() const { return window_count_; }

   private:
    int window_count_;
  };

  explicit TabsCounter(BrowserList* browser_list,
                       SessionRestorationService* service);

  TabsCounter(const TabsCounter&) = delete;
  TabsCounter& operator=(const TabsCounter&) = delete;

  ~TabsCounter() override;

  // browsing_data::BrowsingDataCounter implementation.
  const char* GetPrefName() const override;
  void Count() override;

 private:
  // Callback of `LoadDataFromStorage` with a browser's webstates and their last
  // navigation timestamp. It then increments `total_tab_count_` and
  // `total_window_count_` and reports the result if all callbacks have been
  // received, i.e. if `pending_tasks_count_ == 0`.
  void OnLoadDataFromStorageResult(tabs_closure_util::WebStateIDToTime result);

  // Reports the results to the caller of `TabsCounters`.
  void ReportTabsResult();

  // Resets `total_tab_count_`, `total_window_count_` and `pending_tasks_count_`
  // to zero.
  void ResetCounts();

  // Number of pending tasks.
  int pending_tasks_count_ = 0;

  // Cumulative result of the number of tabs that have a last navigation within
  // the timerange and the number of windows that have such tabs.
  int total_tab_count_ = 0;
  int total_window_count_ = 0;

  // This object is sequence affine.
  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<BrowserList> browser_list_;
  raw_ptr<SessionRestorationService> service_;

  base::WeakPtrFactory<TabsCounter> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_TABS_COUNTER_H_

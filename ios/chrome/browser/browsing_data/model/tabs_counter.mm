// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/tabs_counter.h"

#import "base/functional/bind.h"
#import "components/browsing_data/core/pref_names.h"
#import "ios/chrome/browser/sessions/session_restoration_service.h"
#import "ios/chrome/browser/sessions/session_restoration_service_tmpl.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/web/public/browser_state.h"
#import "net/base/completion_repeating_callback.h"

TabsCounter::TabsCounter(BrowserList* browser_list,
                         SessionRestorationService* service)
    : browser_list_(browser_list), service_(service), weak_ptr_factory_(this) {
  CHECK(browser_list);
  CHECK(service);
}

TabsCounter::~TabsCounter() = default;

const char* TabsCounter::GetPrefName() const {
  return browsing_data::prefs::kCloseTabs;
}

void TabsCounter::Count() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel existing requests and make sure the counters are reset.
  weak_ptr_factory_.InvalidateWeakPtrs();
  ResetCounts();

  // TODO(crbug.com/349074585): Migrate to
  // BrowserList::BrowsersOfType(BrowserList::kRegularAndInactive).
  std::set<Browser*> all_regular_browsers = browser_list_->AllRegularBrowsers();

  // Report result early if browser list is empty.
  if (all_regular_browsers.empty()) {
    ReportTabsResult();
    return;
  }

  for (Browser* browser : all_regular_browsers) {
    pending_tasks_count_++;
    service_->LoadDataFromStorage(
        browser,
        base::BindRepeating(
            &tabs_closure_util::GetLastCommittedTimestampFromStorage),
        base::BindOnce(&TabsCounter::OnLoadDataFromStorageResult,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void TabsCounter::OnLoadDataFromStorageResult(
    tabs_closure_util::WebStateIDToTime result) {
  // Ensure that all callbacks are on the same thread, so that we do not need
  // a mutex for `total_tab_count_`, `total_window_count_` and
  // `pending_tasks_count_`.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/341107836): Cache the tabs info in order to reuse the data
  // when actually closing the tabs.
  int tab_count = tabs_closure_util::GetTabsToClose(result, GetPeriodStart(),
                                                    GetPeriodEnd())
                      .size();
  if (tab_count > 0) {
    total_tab_count_ += tab_count;
    total_window_count_++;
  }

  // Check if all tasks have returned. If not, return early.
  CHECK_GT(pending_tasks_count_, 0);
  if (--pending_tasks_count_ > 0) {
    return;
  }

  // If all tasks have returned, then report the result and reset counts.
  ReportTabsResult();
}

void TabsCounter::ReportTabsResult() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(total_tab_count_, 0);
  CHECK_GE(total_window_count_, 0);

  auto tabs_result = std::make_unique<TabsCounter::TabsResult>(
      this, total_tab_count_, total_window_count_);
  ReportResult(std::move(tabs_result));
  ResetCounts();
}

void TabsCounter::ResetCounts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  total_tab_count_ = 0;
  total_window_count_ = 0;
  pending_tasks_count_ = 0;
}

// TabsCounter::TabsResult -----------------------------------------

TabsCounter::TabsResult::TabsResult(const TabsCounter* source,
                                    ResultInt tab_count,
                                    ResultInt window_count)
    : FinishedResult(source, tab_count), window_count_(window_count) {}

TabsCounter::TabsResult::~TabsResult() = default;

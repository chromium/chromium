// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_IOS_BROWSING_HISTORY_DRIVER_DELEGATE_BRIDGE_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_IOS_BROWSING_HISTORY_DRIVER_DELEGATE_BRIDGE_H_

#import "ios/chrome/browser/history/ui_bundled/history_consumer.h"
#import "ios/chrome/browser/history/ui_bundled/ios_browsing_history_driver_delegate.h"

// Adapter to use an id<HistoryConsumer> as a IOSHistoryDriverDelegate.
class IOSBrowsingHistoryDriverDelegateBridge
    : public IOSBrowsingHistoryDriverDelegate {
 public:
  explicit IOSBrowsingHistoryDriverDelegateBridge(id<HistoryConsumer> delegate);

  IOSBrowsingHistoryDriverDelegateBridge(
      const IOSBrowsingHistoryDriverDelegateBridge&) = delete;
  IOSBrowsingHistoryDriverDelegateBridge& operator=(
      const IOSBrowsingHistoryDriverDelegateBridge&) = delete;

  ~IOSBrowsingHistoryDriverDelegateBridge() override;

  // IOSHistoryDriverDelegate overrides.
  void HistoryQueryCompleted(
      const std::vector<history::BrowsingHistoryService::HistoryEntry>& results,
      const history::BrowsingHistoryService::QueryResultsInfo&
          query_results_info,
      base::OnceClosure continuation_closure) override;
  void HistoryWasDeleted() override;
  void ShowNoticeAboutOtherFormsOfBrowsingHistory(
      BOOL should_show_notice) override;

 private:
  __weak id<HistoryConsumer> delegate_ = nil;
};

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_IOS_BROWSING_HISTORY_DRIVER_DELEGATE_BRIDGE_H_

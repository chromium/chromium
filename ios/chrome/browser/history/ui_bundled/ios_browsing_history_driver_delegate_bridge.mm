// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/ios_browsing_history_driver_delegate_bridge.h"

IOSBrowsingHistoryDriverDelegateBridge::IOSBrowsingHistoryDriverDelegateBridge(
    id<HistoryConsumer> delegate)
    : delegate_(delegate) {}

IOSBrowsingHistoryDriverDelegateBridge::
    ~IOSBrowsingHistoryDriverDelegateBridge() = default;

void IOSBrowsingHistoryDriverDelegateBridge::HistoryQueryCompleted(
    const std::vector<history::BrowsingHistoryService::HistoryEntry>& results,
    const history::BrowsingHistoryService::QueryResultsInfo& query_results_info,
    base::OnceClosure continuation_closure) {
  [delegate_
      historyQueryWasCompletedWithResults:results
                         queryResultsInfo:query_results_info
                      continuationClosure:std::move(continuation_closure)];
}

void IOSBrowsingHistoryDriverDelegateBridge::HistoryWasDeleted() {
  [delegate_ historyWasDeleted];
}

void IOSBrowsingHistoryDriverDelegateBridge::
    ShowNoticeAboutOtherFormsOfBrowsingHistory(BOOL should_show_notice) {
  [delegate_ showNoticeAboutOtherFormsOfBrowsingHistory:should_show_notice];
}

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_CONSUMER_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_CONSUMER_H_

#import <Foundation/Foundation.h>

#include "components/history/core/browser/browsing_history_service.h"

// Defines methods to manage history query results and deletion actions.
@protocol HistoryConsumer <NSObject>

// Tells the consumer that the result of a history query has been retrieved.
// Entries in `result` are already sorted.
- (void)
    historyQueryWasCompletedWithResults:
        (const std::vector<history::BrowsingHistoryService::HistoryEntry>&)
            results
                       queryResultsInfo:(const history::BrowsingHistoryService::
                                             QueryResultsInfo&)queryResultsInfo
                    continuationClosure:(base::OnceClosure)continuationClosure;

// Tells the consumer that history entries have been deleted by a different
// client.
- (void)historyWasDeleted;

// Tells the consumer whether to show notice about other forms of
// browsing history or not.
- (void)showNoticeAboutOtherFormsOfBrowsingHistory:(BOOL)shouldShowNotice;
@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_CONSUMER_H_

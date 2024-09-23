// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_CLEAR_BROWSING_DATA_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_CLEAR_BROWSING_DATA_COORDINATOR_DELEGATE_H_

@class HistoryClearBrowsingDataCoordinator;

// Delegate for the history clear browsing data coordinator.
@protocol HistoryClearBrowsingDataCoordinatorDelegate <NSObject>

// Notifies the coordinator that history should be dismissed.
- (void)dismissHistoryClearBrowsingData:
            (HistoryClearBrowsingDataCoordinator*)coordinator
                         withCompletion:(ProceduralBlock)completionHandler;

// Notifies the coordinator that Privacy Settings should be displayed.
- (void)displayClearHistoryData;

@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_CLEAR_BROWSING_DATA_COORDINATOR_DELEGATE_H_

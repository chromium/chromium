// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#include "base/ios/block_types.h"

@class HistoryTableViewController;

// Protocol to communicate HistoryTableViewController actions to its
// coordinator.
@protocol HistoryTableViewControllerDelegate
// Notifies the coordinator that history should be dismissed.
- (void)dismissHistoryTableViewController:
            (HistoryTableViewController*)controller
                           withCompletion:(ProceduralBlock)completionHandler;
// Notifies the coordinator that Privacy Settings should be displayed.
- (void)displayClearHistoryData;
@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_TABLE_VIEW_CONTROLLER_DELEGATE_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#include "base/ios/block_types.h"

@class BaseHistoryViewController;

// Protocol to communicate HistoryTableViewController actions to its
// coordinator.
@protocol HistoryTableViewControllerDelegate
// Notifies the coordinator that history should be dismissed and calls the
// completion handler block. The completion handler block is called after the
// view controller has been dismissed.
- (void)dismissViewController:(BaseHistoryViewController*)controller
               withCompletion:(ProceduralBlock)completionHandler;
// Notifies the coordinator that history should be dismissed.
- (void)dismissViewController:(BaseHistoryViewController*)controller;
// Notifies the coordinator that Privacy Settings should be displayed.
- (void)displayClearHistoryData;
@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_TABLE_VIEW_CONTROLLER_DELEGATE_H_

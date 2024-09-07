// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

@class TableViewItem;

namespace browsing_data {
enum class TimePeriod;
}

enum class BrowsingDataRemoveMask;

@protocol ClearBrowsingDataConsumer <NSObject>
// Execute action to clear browsing data.
// `completionBlock` is then executed asynchronously.
- (void)removeBrowsingDataForTimePeriod:(browsing_data::TimePeriod)timePeriod
                             removeMask:(BrowsingDataRemoveMask)removeMask
                        completionBlock:(ProceduralBlock)completionBlock;
// Updates contents of a cell for a given item. Set reload to NO if a simple
// reconfigre is enough.
- (void)updateCellsForItem:(TableViewItem*)item reload:(BOOL)reload;

// Indicate to user that data has been cleared.
- (void)showBrowsingHistoryRemovedDialog;

// Dismisses the alert coordinator.
- (void)dismissAlertCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_CONSUMER_H_

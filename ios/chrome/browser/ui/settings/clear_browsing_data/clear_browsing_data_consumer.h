// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_CONSUMER_H_

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"

@class ListItem;

namespace ios {
class ChromeBrowserState;
}

namespace browsing_data {
enum class TimePeriod;
}

enum class BrowsingDataRemoveMask;

@protocol ClearBrowsingDataConsumer <NSObject>
// Execute action to clear browsing data.
- (void)removeBrowsingDataForBrowserState:(ios::ChromeBrowserState*)browserState
                               timePeriod:(browsing_data::TimePeriod)timePeriod
                               removeMask:(BrowsingDataRemoveMask)removeMask
                          completionBlock:(ProceduralBlock)completionBlock;
// Updates contents of a cell for a given item.
- (void)updateCellsForItem:(ListItem*)item;

// Indicate to user that data has been cleared.
- (void)showBrowsingHistoryRemovedDialog;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_CONSUMER_H_

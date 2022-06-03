// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_BROWSING_DATA_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_BROWSING_DATA_COMMANDS_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"

class ChromeBrowserState;

// Protocol for commands that relate to browsing data.
@protocol BrowsingDataCommands<NSObject>

// Remove browsing data for |browserState| for the |timePeriod|. The type of
// data to remove is controlled by |removeMask| (see BrowserDataRemoveMask).
// Once data is removed, |completionBlock| is invoked.
- (void)removeBrowsingDataForBrowserState:(ChromeBrowserState*)browserState
                               timePeriod:(browsing_data::TimePeriod)timePeriod
                               removeMask:(BrowsingDataRemoveMask)removeMask
                          completionBlock:(ProceduralBlock)completionBlock;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_BROWSING_DATA_COMMANDS_H_

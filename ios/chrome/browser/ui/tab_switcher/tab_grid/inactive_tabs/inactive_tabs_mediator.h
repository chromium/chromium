// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_image_data_source.h"

@class SnapshotCache;
@protocol TabCollectionConsumer;
class WebStateList;

// This mediator provides data to the Inactive Tabs grid and handles
// interactions.
@interface InactiveTabsMediator : NSObject <GridImageDataSource>

// Initializer with `consumer` as the receiver of `webStateList` updates.
- (instancetype)initWithConsumer:(id<TabCollectionConsumer>)consumer
                    webStateList:(WebStateList*)webStateList
                   snapshotCache:(SnapshotCache*)snapshotCache
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Tells the receiver to close the item with the `itemID` identifier.
- (void)closeItemWithID:(NSString*)itemID;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_MEDIATOR_H_

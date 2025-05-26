// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_PINNED_TABS_PINNED_TABS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_PINNED_TABS_PINNED_TABS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item_snapshot_and_favicon_data_source.h"

class Browser;
@protocol PinnedTabCollectionConsumer;

// Mediates between model layer and pinned tabs collection UI layer.
@interface PinnedTabsMediator
    : NSObject <TabCollectionDragDropHandler,
                TabSwitcherItemSnapShotAndFaviconDataSource>

// The source browser.
@property(nonatomic, assign) Browser* browser;

// Initializer with `consumer` as the receiver of model layer updates.
- (instancetype)initWithConsumer:(id<PinnedTabCollectionConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_PINNED_TABS_PINNED_TABS_MEDIATOR_H_

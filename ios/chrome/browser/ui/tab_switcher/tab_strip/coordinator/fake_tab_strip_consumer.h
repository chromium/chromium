// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_COORDINATOR_FAKE_TAB_STRIP_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_COORDINATOR_FAKE_TAB_STRIP_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift.h"

// Fake consumer to get the passed value in tests.
@interface FakeTabStripConsumer : NSObject <TabStripConsumer>

@property(nonatomic, strong) NSMutableArray<TabStripItemIdentifier*>* items;
@property(nonatomic, strong) TabSwitcherItem* selectedItem;
@property(nonatomic, copy) NSArray<TabStripItemIdentifier*>* reconfiguredItems;
@property(nonatomic, strong)
    NSMutableDictionary<TabStripItemIdentifier*, TabStripItemData*>* itemData;
@property(nonatomic, strong)
    NSMutableDictionary<TabStripItemIdentifier*, TabGroupItem*>* itemParents;
@property(nonatomic, strong)
    NSMutableSet<TabStripItemIdentifier*>* expandedItems;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_COORDINATOR_FAKE_TAB_STRIP_CONSUMER_H_

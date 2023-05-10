// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_PRIVATE_H_

#import <UIKit/UIKit.h>

@class TabSwitcherItem;

@interface GridViewController (Private)

@property(nonatomic, readonly) NSMutableArray<TabSwitcherItem*>* items;
@property(nonatomic, readonly) NSUInteger selectedIndex;
@property(nonatomic, readonly) UICollectionView* collectionView;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_PRIVATE_H_

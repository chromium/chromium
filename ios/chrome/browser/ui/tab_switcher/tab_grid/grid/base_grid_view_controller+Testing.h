// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_TESTING_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller+subclassing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"

@class TabSwitcherItem;

// Testing category to expose private properties used for tests only.
@interface BaseGridViewController (Testing)

@property(nonatomic, readonly) GridDiffableDataSource* diffableDataSource;
@property(nonatomic, readonly) NSUInteger selectedIndex;
@property(nonatomic, readonly) UICollectionView* collectionView;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_TESTING_H_

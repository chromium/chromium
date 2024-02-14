// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_BOTTOM_TRAILING_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_BOTTOM_TRAILING_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_grid_configurable_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"

// A square-ish view in a bottom trailing side of a group cell. Contains either
// a main subview, which is a snapshot plus a favicon or multiple UIImage each
// one presenting a favicon or the number of tabs left in the group.
@interface GroupGridBottomTrailingView : GroupGridConfigurableView

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithSpacing:(CGFloat)spacing
       adaptForCompactSizeClass:(BOOL)adaptForCompactSizeClass NS_UNAVAILABLE;

// Configures the views with the corresponding favicons and if
// `remainingTabsCount` is not equal to zero configure the bottom trailing view
// with it.
- (void)configureWithFavicons:(NSArray<UIImage*>*)favicons
           remainingTabsCount:(NSInteger)remainingTabsCount;

// Configures the main view with the corresponding snapshot and favicon, when
// only one tab is displayed.
- (void)configureWithGroupTabInfo:(GroupTabInfo*)groupTabInfo;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_GRID_BOTTOM_TRAILING_VIEW_H_

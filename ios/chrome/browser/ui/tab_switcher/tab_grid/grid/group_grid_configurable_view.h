// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_GRID_CONFIGURABLE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_GRID_CONFIGURABLE_VIEW_H_

#import <UIKit/UIKit.h>

@class GroupTabInfo;

// `GroupGridConfigurableView` is a UIView with 4 subviews distributed equally,
// topleading/topTrailing/bottomLeading/bottomTrailing.
@interface GroupGridConfigurableView : UIView

// Designated initializer with `isGroupView` to configure the view, when `NO`
// the view will be configured as a bottom trailing view that displays and the
// count of the remaining tabs in the group when `YES the view will be
// configured as a main group view.
- (instancetype)initWithIsMainGroupView:(BOOL)isMainGroupView
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Configures the subviews with a given snapshot/favicon pairs and
// passes the total tabs count to the bottomTrailingView.
- (void)configureWithGroupTabInfos:(NSArray<GroupTabInfo*>*)groupTabInfos
                    totalTabsCount:(NSInteger)totalTabsCount;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_GRID_CONFIGURABLE_VIEW_H_

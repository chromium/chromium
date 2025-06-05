// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_SNAPSHOTS_VIEW_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_SNAPSHOTS_VIEW_H_

#import <UIKit/UIKit.h>

@class GroupTabView;
@class TabSnapshotAndFavicon;

// View that manages the tab group sample view with multiples snapshots.
@interface TabGroupSnapshotsView : UIView

// Number of tabs displayed in the snapshot
@property(nonatomic, assign) NSInteger tabsCount;

// Initializes the view.
// - `isLight`: `YES` for light UI interface.
// - `isCell`: `YES` if the view is used as a cell.
- (instancetype)initWithLightInterface:(BOOL)isLight
                                  cell:(BOOL)isCell NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

- (void)configureTabSnapshotAndFavicon:
            (TabSnapshotAndFavicon*)tabSnapshotAndFavicon
                              tabIndex:(NSInteger)tabIndex;

// Returns all tab views that compose this tab group view in the order they're
// presented.
- (NSArray<GroupTabView*>*)allGroupTabViews;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_SNAPSHOTS_VIEW_H_

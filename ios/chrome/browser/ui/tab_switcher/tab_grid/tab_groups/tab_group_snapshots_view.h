// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_SNAPSHOTS_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_SNAPSHOTS_VIEW_H_

#import <UIKit/UIKit.h>

@class GroupTabInfo;
@class GroupTabView;

// View controller that manages the tab group sample view with multiples
// snapshots.
@interface TabGroupSnapshotsView : UIView

- (instancetype)initWithTabGroupInfos:(NSArray<GroupTabInfo*>*)tabGroupInfos
                                 size:(NSUInteger)size
                                light:(BOOL)isLight
                                 cell:(BOOL)isCell;

- (void)configureTabGroupSnapshotsViewWithTabGroupInfos:
            (NSArray<GroupTabInfo*>*)tabGroupInfos
                                                   size:(NSUInteger)size;

// Returns all tab views that compose this tab group view in the order they're
// presented.
- (NSArray<GroupTabView*>*)allGroupTabViews;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_SNAPSHOTS_VIEW_H_

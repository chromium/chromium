// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_BOTTOM_TRAILING_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_BOTTOM_TRAILING_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"

// A square-ish view in a bottom trailing side of a group cell. Contains either
// a main subview, which is a snapshot plus a favicon or multiple UIImage each
// one presenting a favicon or the number of tabs left in the group.
@interface GroupGridBottomTrailingView : UIView

// The main snapshot view when the number of tabs is equal to 1.
@property(nonatomic, weak) GroupTabInfo* mainSubviewImageAndFavicon;
// The favicons to display when the number of tabs exceeds 1.
@property(nonatomic, strong) NSArray<UIImage*>* favicons;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_GRID_BOTTOM_TRAILING_VIEW_H_

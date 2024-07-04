// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_H_

#import <UIKit/UIKit.h>

namespace base {
class Time;
}  // namespace base

// Represents a tab group in the Tab Groups panel in Tab Grid.
// TODO(crbug.com/350493712): Implement equality based on SavedTabGroupID.
// Currently, it compares `title`.
@interface TabGroupsPanelItem : NSObject

// The title of the Tab Group.
@property(nonatomic, copy) NSString* title;

// The color of the dot.
@property(nonatomic, strong) UIColor* color;

// The creation date of the Tab Group.
@property(nonatomic, assign) base::Time creationDate;

// The favicons of the tabs in the Tab Group.
@property(nonatomic, copy) NSArray<UIImage*>* favicons;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_H_

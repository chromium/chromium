// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_CELL_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_cell.h"

@class TabStripGroupCell;

// Informs the receiver of actions on the cell.
@protocol TabStripGroupCellDelegate
// Informs the receiver that the expand or collapse selector has been tapped.
- (void)collapseOrExpandTappedForCell:(TabStripGroupCell*)cell;
@end

// TabStripCell that contains a group title.
@interface TabStripGroupCell : TabStripCell

// Delegate to inform the TabStrip on the cell.
@property(nonatomic, weak) id<TabStripGroupCellDelegate> delegate;

// Background color of the title container.
@property(nonatomic, strong) UIColor* titleContainerBackgroundColor;

// Color of the title.
@property(nonatomic, strong) UIColor* titleTextColor;

// Whether the cell is that of a collapsed group. Default value is NO.
@property(nonatomic, assign) BOOL collapsed;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_CELL_H_

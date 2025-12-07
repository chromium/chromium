// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_OUT_OF_DATE_MESSAGE_CELL_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_OUT_OF_DATE_MESSAGE_CELL_H_

#import <UIKit/UIKit.h>

@class TabGroupsPanelItem;
@class TabGroupsPanelOutOfDateMessageCell;

// Delegate protocol for the tab groups panel out-of-date message cell.
@protocol TabGroupsPanelOutOfDateMessageCellDelegate

// Notifies the delegate that the update button in the out-of-date message cell
// was tapped.
- (void)updateButtonTappedForOutOfDateMessageCell:
    (TabGroupsPanelOutOfDateMessageCell*)outOfDateMessageCell;

// Notifies the delegate that the close button in the out-of-date message cell
// was tapped.
- (void)closeButtonTappedForOutOfDateMessageCell:
    (TabGroupsPanelOutOfDateMessageCell*)outOfDateMessageCell;

@end

// Represents the potential out-of-date message at the top of the tab groups
// panel.
@interface TabGroupsPanelOutOfDateMessageCell : UICollectionViewCell

// Delegate that gets called when the button are tapped.
@property(nonatomic, weak) id<TabGroupsPanelOutOfDateMessageCellDelegate>
    delegate;

// Associated item, identifying the represented out-of-date message. The item
// must have a type of `TabGroupsPanelItemType::kOutOfDateMessage`.
@property(nonatomic, strong) TabGroupsPanelItem* outOfDateMessageItem;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_OUT_OF_DATE_MESSAGE_CELL_H_

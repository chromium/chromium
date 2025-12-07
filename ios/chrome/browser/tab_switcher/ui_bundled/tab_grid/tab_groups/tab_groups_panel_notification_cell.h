// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_NOTIFICATION_CELL_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_NOTIFICATION_CELL_H_

#import <UIKit/UIKit.h>

@class TabGroupsPanelItem;

// Delegate protocol for the Tab Groups panel notification cells.
@protocol TabGroupsPanelNotificationCellDelegate

// Notifies the delegate that the close button in a notification cell was
// tapped.
- (void)closeButtonTappedForNotificationItem:
    (TabGroupsPanelItem*)notificationItem;

@end

// Represents a notification at the top of the Tab Groups panel.
@interface TabGroupsPanelNotificationCell : UICollectionViewCell

// Delegate that gets called when the close button is tapped.
@property(nonatomic, weak) id<TabGroupsPanelNotificationCellDelegate> delegate;

// Associated item, identifying the represented notification. The item must have
// a type of `TabGroupsPanelItemType::kNotification`.
@property(nonatomic, strong) TabGroupsPanelItem* notificationItem;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_NOTIFICATION_CELL_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_CELL_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_CELL_H_

#import "ios/chrome/browser/tab_switcher/tab_strip/ui/tab_strip_cell.h"

@protocol FacePileProviding;
@class TabStripGroupCell;

// Informs the receiver of actions on the cell.
@protocol TabStripGroupCellDelegate
// Informs the receiver that the expand or collapse selector has been tapped.
- (void)collapseOrExpandTappedForCell:(TabStripGroupCell*)cell;
@end

// TabStripCell that contains a group title.
@interface TabStripGroupCell : TabStripCell

// Returns the approximative width of a TabStripGroupCell for the given `title`.
// This computation doesn't take into account the face pile.
+ (CGFloat)approximativeNonSharedWidthWithTitle:(NSString*)title;

// Delegate to inform the TabStrip on the cell.
@property(nonatomic, weak) id<TabStripGroupCellDelegate> delegate;

// Background color of the content container.
@property(nonatomic, strong) UIColor* contentContainerBackgroundColor;

// Color of the title.
@property(nonatomic, strong) UIColor* titleTextColor;

// Whether the cell is that of a collapsed group. Default value is NO.
@property(nonatomic, assign) BOOL collapsed;

// Whether this cell has a notification dot.
@property(nonatomic, assign) BOOL hasNotificationDot;

// The width that this cell would take if there is no width constraints (fitting
// all the text).
@property(nonatomic, readonly) CGFloat optimalWidth;

// The FacePileProvider, to be set externally. Held as a strong reference to
// ensure the provider's lifecycle is maintained for managing and updating the
// FacePileView's content.
@property(nonatomic, strong) id<FacePileProviding> facePileProvider;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_CELL_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_CELL_H_

#import <UIKit/UIKit.h>

@class TabStripCell;

// Informs the receiver of actions on the cell.
@protocol TabStripCellDelegate
// Informs the receiver that the close button on the cell was tapped.
- (void)closeButtonTappedForCell:(TabStripCell*)cell;
@end

// UICollectionViewCell that contains a Tab title with a leading imageView
// and a close tab button.
@interface TabStripCell : UICollectionViewCell

// The close button associated with this cell.
@property(nonatomic, strong) UIButton* closeButton;
// Title is displayed by this label.
@property(nonatomic, strong) UILabel* titleLabel;
// View for displaying the favicon.
@property(nonatomic, strong) UIImageView* faviconView;
// Unique identifier for the cell's contents. This is used to ensure that
// updates in an asynchronous callback are only made if the item is the same.
@property(nonatomic, copy) NSString* itemIdentifier;
// Delegate to inform the TabStrip on the cell.
@property(nonatomic, weak) id<TabStripCellDelegate> delegate;

// Checks if cell has a specific identifier.
- (BOOL)hasIdentifier:(NSString*)identifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_CELL_H_

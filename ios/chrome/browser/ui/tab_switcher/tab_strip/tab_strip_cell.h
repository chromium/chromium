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

// Delegate to inform the TabStrip on the cell.
@property(nonatomic, weak) id<TabStripCellDelegate> delegate;

// Whether the associated tab is loading.
@property(nonatomic, assign) BOOL loading;

// Sets the title of the cell.
- (void)setTitle:(NSString*)title;
// Sets the favicon for the page. Passing nil sets the default image.
- (void)setFaviconImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_CELL_H_

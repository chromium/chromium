// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_TAB_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_TAB_CELL_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_cell.h"

#import <UIKit/UIKit.h>

@class TabStripTabCell;

// Informs the receiver of actions on the cell.
@protocol TabStripTabCellDelegate
// Informs the receiver that the close button on the cell was tapped.
- (void)closeButtonTappedForCell:(TabStripTabCell*)cell;
@end

// TabStripCell that contains a Tab title with a leading imageView and a close
// tab button.
@interface TabStripTabCell : TabStripCell <UIPointerInteractionDelegate>

// Delegate to inform the TabStrip on the cell.
@property(nonatomic, weak) id<TabStripTabCellDelegate> delegate;

// Whether the associated tab is loading.
@property(nonatomic, assign) BOOL loading;

// Whether the cell leading separator is hidden.
@property(nonatomic, assign) BOOL leadingSeparatorHidden;

// Whether the cell trailing separator is hidden.
@property(nonatomic, assign) BOOL trailingSeparatorHidden;

// Whether the cell leading separator gradient view is hidden.
@property(nonatomic, assign) BOOL leadingSeparatorGradientViewHidden;

// Whether the cell trailing separator gradient view is hidden.
@property(nonatomic, assign) BOOL trailingSeparatorGradientViewHidden;

// Whether the left background view of the selected cell is hidden.
@property(nonatomic, assign) BOOL leadingSelectedBorderBackgroundViewHidden;

// Whether the right background view of the selected cell is hidden.
@property(nonatomic, assign) BOOL trailingSelectedBorderBackgroundViewHidden;

// Whether the cell is the first of its group. Default value is NO.
@property(nonatomic, assign) BOOL isFirstTabInGroup;

// Whether the cell is the last of its group. Default value is NO.
@property(nonatomic, assign) BOOL isLastTabInGroup;

// The item associated with this cell. Passed as an opaque NSObject to use the
// isEqual method on it.
@property(nonatomic, weak) NSObject* item;

// Used to know the position of the tab and the total number of tabs. This is
// used by VoiceOver to let the user know the position of the tab and should not
// be used elsewhere.
@property(nonatomic, assign) NSInteger tabIndex;
@property(nonatomic, assign) NSInteger numberOfTabs;

// Sets the title of the cell.
- (void)setTitle:(NSString*)title;
// Sets the favicon for the page. Passing nil sets the default image.
- (void)setFaviconImage:(UIImage*)image;

// Sets the height of the separators.
- (void)setSeparatorsHeight:(CGFloat)height;

// Sets the visibility of the close button.
- (void)setCloseButtonVisibility:(CGFloat)visibility;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_TAB_CELL_H_

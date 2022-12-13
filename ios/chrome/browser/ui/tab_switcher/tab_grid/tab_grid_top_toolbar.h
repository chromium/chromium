// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_TOP_TOOLBAR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_TOP_TOOLBAR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

@class TabGridPageControl;

// Top toolbar for TabGrid. The appearance of the toolbar is decided by screen
// size, current TabGrid page and mode:
//
// Horizontal-compact and vertical-regular screen size:
//   Normal mode:    [               PageControl      Select]
//   Remote page:    [               PageControl            ]
//   Selection mode: [SelectAll    SelectedTabsCount    Done]
// Other screen size:
//   Normal mode:    [CloseAll           PageControl      Select Done]
//   Remote page:    [                   PageControl             Done]
//   Selection mode: [SelectAll        SelectedTabsCount         Done]
@interface TabGridTopToolbar : UIToolbar

// These components are publicly available to allow the user to set their
// contents, visibility and actions.
@property(nonatomic, strong, readonly) UIBarButtonItem* anchorItem;
@property(nonatomic, strong, readonly) TabGridPageControl* pageControl;
// This property together with `mode` control the items shown in toolbar.
@property(nonatomic, assign) TabGridPage page;
// TabGrid mode, it controls the items shown in toolbar.
@property(nonatomic, assign) TabGridMode mode;
// This property indicates the count of selected tabs when the tab grid is in
// selection mode. It will be used to update the buttons to use the correct
// title (singular or plural).
@property(nonatomic, assign) int selectedTabsCount;

// Sets target/action for tapping event on new tab button.
- (void)setNewTabButtonTarget:(id)target action:(SEL)action;
// Sets target/action for tapping event on select all button.
- (void)setSelectAllButtonTarget:(id)target action:(SEL)action;
// Sets target/action for tapping event on close all button.
- (void)setCloseAllButtonTarget:(id)target action:(SEL)action;
// Sets target/action for tapping event on done button.
- (void)setDoneButtonTarget:(id)target action:(SEL)action;
// Sets target/action for tapping event on search button.
- (void)setSearchButtonTarget:(id)target action:(SEL)action;
// Sets target/action for tapping event on cancel search button.
- (void)setCancelSearchButtonTarget:(id)target action:(SEL)action;
// Sets the delegate for the searchbar.
- (void)setSearchBarDelegate:(id<UISearchBarDelegate>)delegate;
// Set `enabled` on the search button.
- (void)setSearchButtonEnabled:(BOOL)enabled;
// Set `enabled` on the new tab button.
- (void)setNewTabButtonEnabled:(BOOL)enabled;
// Set `enabled` on the select all button.
- (void)setSelectAllButtonEnabled:(BOOL)enabled;
// Set `enabled` on the done button.
- (void)setDoneButtonEnabled:(BOOL)enabled;
// Set `enabled` on the close all button.
- (void)setCloseAllButtonEnabled:(BOOL)enabled;
// use undo or closeAll text on the close all button based on `useUndo` value.
- (void)useUndoCloseAll:(BOOL)useUndo;

// Sets the `menu` displayed on tapping the Edit button.
- (void)setEditButtonMenu:(UIMenu*)menu API_AVAILABLE(ios(14.0));
// Set `enabled` on the Edit button.
- (void)setEditButtonEnabled:(BOOL)enabled;

// Sets the title of the Select All button to "Deselect All".
- (void)configureDeselectAllButtonTitle;
// Sets the title of the Select All button to "Select All".
- (void)configureSelectAllButtonTitle;

// Hides components and uses a black background color for tab grid transition
// animation.
- (void)hide;
// Recovers the normal appearance for tab grid transition animation.
- (void)show;
// Updates the appearance of the this toolbar, based on whether the content
// below it is `scrolledToEdge` or not.
- (void)setScrollViewScrolledToEdge:(BOOL)scrolledToEdge;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_TOP_TOOLBAR_H_

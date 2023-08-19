// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_BOTTOM_TOOLBAR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_BOTTOM_TOOLBAR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/keyboard/key_command_actions.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

@class TabGridNewTabButton;
@protocol TabGridToolbarsButtonsDelegate;

// Bottom toolbar for TabGrid. The appearance of the toolbar is decided by
// screen size, current TabGrid page and mode:
//
// Horizontal-compact and vertical-regular screen size:
//   Small newTabButton, translucent background.
//   Incognito & Regular page: [CloseAllButton  newTabButton  DoneButton]
//   Remote page:              [                              DoneButton]
//   Selection mode:           [CloseTabButton  shareButton  AddToButton]
//
// Other screen size:
//   Large newTabButton, floating layout without UIToolbar.
//   Normal mode:    [                                      newTabButton]
//   Remote page:    [                                                  ]
//   Selection mode: [CloseTabButton       shareButton       AddToButton]
@interface TabGridBottomToolbar : UIView <KeyCommandActions>

// This property together with `mode` and self.traitCollection control the items
// shown in toolbar and its background color. Setting this property will also
// set it on `newTabButton`.
@property(nonatomic, assign) TabGridPage page;
// This property together with `page` and self.traitCollection control the
// items shown in toolbar and its background color.
@property(nonatomic, assign) TabGridMode mode;
// This property indicates the count of selected tabs when the tab grid is in
// selection mode. It will be used to update the buttons to use the correct
// title (singular or plural).
@property(nonatomic, assign) int selectedTabsCount;
// Tab button views created for the bottom toolbar.
@property(nonatomic, readonly) TabGridNewTabButton* smallNewTabButton;
@property(nonatomic, readonly) TabGridNewTabButton* largeNewTabButton;
// Delegate to call when a button is pushed.
@property(nonatomic, weak) id<TabGridToolbarsButtonsDelegate> buttonsDelegate;

// Set `enabled` on the new tab button.
- (void)setNewTabButtonEnabled:(BOOL)enabled;
// Set `enabled` on the done button.
- (void)setDoneButtonEnabled:(BOOL)enabled;
// Set `enabled` on the closeAll button.
- (void)setCloseAllButtonEnabled:(BOOL)enabled;
// use undo or closeAll text on the close all button based on `useUndo` value.
- (void)useUndoCloseAll:(BOOL)useUndo;

// Set `enabled` on the close tabs button.
- (void)setCloseTabsButtonEnabled:(BOOL)enabled;

// Set `enabled` on the close tabs button.
- (void)setShareTabsButtonEnabled:(BOOL)enabled;

// Sets the `menu` displayed on tapping the Add To button.
- (void)setAddToButtonMenu:(UIMenu*)menu;
// Set `enabled` on the Add To button.
- (void)setAddToButtonEnabled:(BOOL)enabled;

// Sets the `menu` displayed on tapping the Edit button.
- (void)setEditButtonMenu:(UIMenu*)menu;
// Set `enabled` on the Edit button.
- (void)setEditButtonEnabled:(BOOL)enabled;

// Hides components and uses a black background color for tab grid transition
// animation.
- (void)hide;
// Recovers the normal appearance for tab grid transition animation.
- (void)show;
// Updates the appearance of the this toolbar, based on whether the content
// below it is `scrolledToEdge` or not.
- (void)setScrollViewScrolledToEdge:(BOOL)scrolledToEdge;
// Adds the receiver in the chain before the original next responder.
- (void)respondBeforeResponder:(UIResponder*)nextResponder;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_BOTTOM_TOOLBAR_H_

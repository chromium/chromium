// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_TOP_TOOLBAR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_TOP_TOOLBAR_H_

#import <UIKit/UIKit.h>

@class TabGridPageControl;

// Top toolbar for TabGrid. In horizontal-compact and vertical-regular screen
// size, the toolbar shows 3 components, with two text buttons on each side and
// a TabGridPageControl in the middle. For other screen sizes, the toolbar only
// shows the newTabButton on the right. The toolbar always has a translucent
// background.
@interface TabGridTopToolbar : UIToolbar
// These components are publicly available to allow the user to set their
// contents, visibility and actions.
@property(nonatomic, strong, readonly) UIBarButtonItem* leadingButton;
@property(nonatomic, strong, readonly) UIBarButtonItem* trailingButton;
@property(nonatomic, strong, readonly) TabGridPageControl* pageControl;

// Sets target/action for tapping event on new tab button.
- (void)setNewTabButtonTarget:(id)target action:(SEL)action;
// Set |enabled| on the new tab button.
- (void)setNewTabButtonEnabled:(BOOL)enabled;

// Hides components and uses a black background color for tab grid transition
// animation.
- (void)hide;
// Recovers the normal appearance for tab grid transition animation.
- (void)show;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_TOP_TOOLBAR_H_

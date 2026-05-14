// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_TAB_GRID_BADGE_BUTTON_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_TAB_GRID_BADGE_BUTTON_H_

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"

// Button displaying the tab count overlay inside a tab grid symbol.
@interface ToolbarTabGridBadgeButton : ToolbarButton

// The number of tabs to display in the overlay.
@property(nonatomic, assign) NSUInteger tabCount;

// Sets whether the active tab is part of a group, updating the symbol layout.
@property(nonatomic, assign) BOOL inTabGroup;

// The visible path of the button's icon, used for custom targeted previews.
- (UIBezierPath*)visiblePath;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_TAB_GRID_BADGE_BUTTON_H_

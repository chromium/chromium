// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBAR_BACKGROUND_VIEW_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBAR_BACKGROUND_VIEW_H_

#import <UIKit/UIKit.h>

// The position of the toolbar background view.
enum class TabGridToolbarBackgroundPosition {
  kTop,
  kBottom,
};

// A view that displays the background for the tab grid toolbars.
@interface TabGridToolbarBackgroundView : UIView

@property(nonatomic, assign) CGFloat remainingScrollDistance;

// Initializes the toolbar background view with the given position.
- (instancetype)initWithPosition:(TabGridToolbarBackgroundPosition)position
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBAR_BACKGROUND_VIEW_H_

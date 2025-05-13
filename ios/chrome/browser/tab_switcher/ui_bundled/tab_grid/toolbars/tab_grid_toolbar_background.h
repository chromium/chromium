// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBAR_BACKGROUND_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBAR_BACKGROUND_H_

#import <UIKit/UIKit.h>

@interface TabGridToolbarBackground : UIView

// Sets whether the blur effect over content background view should be hidden.
- (void)setScrolledOverContentBackgroundViewHidden:(BOOL)hidden;

// Sets whether the solid scrolled to edge background view should be hidden.
- (void)setScrolledToEdgeBackgroundViewHidden:(BOOL)hidden;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBAR_BACKGROUND_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_SUBCLASSING_H_

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"

/// Protected interface of the AdaptiveToolbarViewController.
@interface AdaptiveToolbarViewController (Subclassing)

/// Reacts to user tapping `collapsedToolbarButton`.
- (void)collapsedToolbarButtonTapped;

/// Sets location bar view controller. Used to move the location bar between the
/// adaptive toolbars. Set to nil to remove from toolbar.
- (void)setLocationBarViewController:
    (UIViewController*)locationBarViewController;

/// Returns the page's theme color. Only available when kThemeColorInTopToolbar
/// flag is enabled.
- (UIColor*)pageThemeColor;

/// Returns the under page background color. Only available when
/// kThemeColorInTopToolbar flag is enabled.
- (UIColor*)underPageBackgroundColor;

/// Updates the toolbar background color.
- (void)updateBackgroundColor;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_SUBCLASSING_H_

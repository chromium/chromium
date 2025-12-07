// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_NAVIGATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_NAVIGATION_DELEGATE_H_

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_constants.h"

// The side swipe navigation delegate.
@protocol SideSwipeNavigationDelegate

// Checks if navigation is possible in the specified direction.
- (BOOL)canNavigateInDirection:(NavigationDirection)direction;

// Determines if a swipe gesture in the given direction will trigger an overlay
// display.
- (BOOL)isSwipingToAnOverlay:(UISwipeGestureRecognizerDirection)direction;

// Returns a snapshot image representing the visual overlay that will appear
// during a swipe navigation in the given direction.
- (UIImage*)swipeNavigationSnapshotForDirection:
    (UISwipeGestureRecognizerDirection)direction;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_NAVIGATION_DELEGATE_H_

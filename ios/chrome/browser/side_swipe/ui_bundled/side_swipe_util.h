// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UTIL_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UTIL_H_

#import <UIKit/UIKit.h>

namespace web {
class NavigationItem;
class WebState;
}

// If swiping to the right (or left in RTL).
BOOL IsSwipingBack(UISwipeGestureRecognizerDirection direction);

// If swiping to the left (or right in RTL).
BOOL IsSwipingForward(UISwipeGestureRecognizerDirection direction);

// Returns `YES` if the item should use Chromium native swipe.  This is true for
// the NTP and chrome://crash.
BOOL UseNativeSwipe(web::NavigationItem* item);

// Determines if swiping back on the given WebState should trigger the Lens
// overlay.
BOOL SwipingBackLeadsToLensOverlay(web::WebState* activeWebState);

// Returns 'YES' if the swipe will lead to an overlay (e.g lens overlay
// feature).
BOOL IsSwipingToAnOverlay(UISwipeGestureRecognizerDirection direction,
                          web::WebState* currentWebState);

// Returns a snapshot image for a swipe navigation transition in a given
// direction.
UIImage* SwipeNavigationSnapshot(UISwipeGestureRecognizerDirection direction,
                                 web::WebState* currentWebState);

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_UTIL_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_CONSTANTS_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace base {
class TimeDelta;
}  // namespace base

// Accessibility identifier for the background in the view.
extern NSString* const kGestureInProductHelpViewBackgroundAXId;

// Accessibility identifier for the bubble in the view.
extern NSString* const kGestureInProductHelpViewBubbleAXId;

// Accessibility identifier for the dismiss button.
extern NSString* const kGestureInProductHelpViewDismissButtonAXId;

// Time for the view to fade in/out from the screen.
extern const base::TimeDelta kGestureInProductHelpViewAppearDuration;

// Time after a cycle completes and before the next cycle in the opposite
// direction begins; only used for bidirectional in-product help gesture views.
extern const base::TimeDelta kDurationBetweenBidirectionalCycles;

// The radius of the gesture indicator when it's animating the user's finger
// movement.
extern const CGFloat kGestureIndicatorRadius;

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_GESTURE_IPH_GESTURE_IN_PRODUCT_HELP_CONSTANTS_H_

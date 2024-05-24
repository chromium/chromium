// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/gesture_iph/gesture_in_product_help_view_egtest_utils.h"

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/ui/bubble/gesture_iph/gesture_in_product_help_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

BOOL HasGestureIPHAppeared() {
  // Disable scoped synchronization to perform checks with animation running.
  ScopedSynchronizationDisabler sync_disabler;
  // Wait for the time it takes for the in-product help to appear/disappear with
  // an extra buffer to generate blurred background.
  base::test::ios::SpinRunLoopWithMinDelay(
      kGestureInProductHelpViewAppearDuration + base::Milliseconds(100));
  return
      [ChromeEarlGrey
          testUIElementAppearanceWithMatcher:
              grey_accessibilityID(kGestureInProductHelpViewBackgroundAXId)] &&
      [ChromeEarlGrey
          testUIElementAppearanceWithMatcher:
              grey_accessibilityID(kGestureInProductHelpViewBubbleAXId)];
}

void TapDismissButton() {
  ScopedSynchronizationDisabler sync_disabler;
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kGestureInProductHelpViewDismissButtonAXId)]
      performAction:grey_tap()];
}

void SwipeIPHInDirection(GREYDirection direction, BOOL edge_swipe) {
  ScopedSynchronizationDisabler sync_disabler;

  CGFloat xOriginStartPercentage = 0.5f;
  CGFloat yOriginStartPercentage = 0.5f;
  if (edge_swipe) {
    switch (direction) {
      case kGREYDirectionUp:
        yOriginStartPercentage = 0.99f;
        break;
      case kGREYDirectionDown:
        yOriginStartPercentage = 0;
        break;
      case kGREYDirectionLeft:
        xOriginStartPercentage = 0.99f;
        break;
      case kGREYDirectionRight:
        xOriginStartPercentage = 0;
        break;
    }
  }
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kGestureInProductHelpViewBackgroundAXId)]
      performAction:grey_swipeFastInDirectionWithStartPoint(
                        direction, xOriginStartPercentage,
                        yOriginStartPercentage)];
}

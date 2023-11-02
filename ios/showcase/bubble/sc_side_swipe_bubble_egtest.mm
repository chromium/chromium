// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/ui/bubble/side_swipe_bubble/side_swipe_bubble_constants.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/showcase/bubble/constants.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/scoped_disable_fast_animation_earl_grey.h"

namespace {

using ::base::test::ios::kWaitForUIElementTimeout;
using ::base::test::ios::WaitUntilConditionOrTimeout;
using ::showcase_utils::Close;
using ::showcase_utils::Open;

// Element matchers for elements in a side swipe bubble view.
id<GREYMatcher> SideSwipeBubble() {
  return grey_accessibilityID(kSideSwipeBubbleViewBubbleAXId);
}
id<GREYMatcher> Background() {
  return grey_accessibilityID(kSideSwipeBubbleViewBackgroundAXId);
}
id<GREYMatcher> DismissButton() {
  return grey_accessibilityID(kSideSwipeBubbleViewDismissButtonAXId);
}

// Matchers for elements confirming how the side swipe bubble is dismissed.
id<GREYMatcher> DismissedDueToTimeOut() {
  return grey_accessibilityLabel(kSideSwipeBubbleViewTimeoutText);
}
id<GREYMatcher> DismissedWithTap() {
  return grey_accessibilityLabel(kSideSwipeBubbleViewDismissedByTapText);
}

}  // namespace

// Tests for the SideSwipeBubbleViewController.
@interface SCSideSwipeBubbleTestCase : ShowcaseTestCase
@end

@implementation SCSideSwipeBubbleTestCase

// Tests that the side swipe bubble shows, and its "Dismiss" button works as
// intended.
- (void)testDismissButtonRemovesTheSideSwipeIPH {
  ScopedDisableFastAnimationEarlGrey fast_animation_disabler;
  Open(@"SideSwipeBubbleView");
  {
    // Disable scoped synchronization to perform checks with animation running.
    ScopedSynchronizationDisabler sync_disabler;

    ConditionBlock condition = ^{
      NSError* error = nil;
      [[EarlGrey selectElementWithMatcher:Background()]
          assertWithMatcher:grey_sufficientlyVisible()
                      error:&error];
      return !error;
    };
    GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
               @"IPH not shown.");

    [[EarlGrey selectElementWithMatcher:SideSwipeBubble()]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:DismissButton()]
        performAction:grey_tap()];
  }
  [[EarlGrey selectElementWithMatcher:Background()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SideSwipeBubble()]
      assertWithMatcher:grey_nil()];
  // Test that the correct dismissal reason is passed in the callback.
  [[EarlGrey selectElementWithMatcher:DismissedWithTap()]
      assertWithMatcher:grey_notNil()];
  Close();
}

// Tests that the side swipe bubble auto-dismisses after three iterations of
// animation (~10s).
- (void)testSideSwipeIPHAutoDismissesAfterTimeOut {
  ScopedDisableFastAnimationEarlGrey fast_animation_disabler;
  base::TimeDelta timeout = base::Seconds(10);

  Open(@"SideSwipeBubbleView");
  {
    // Disable scoped synchronization to perform checks with animation running.
    ScopedSynchronizationDisabler sync_disabler;

    ConditionBlock sideSwipeBubbleViewVisible = ^{
      NSError* error = nil;
      [[EarlGrey selectElementWithMatcher:Background()]
          assertWithMatcher:grey_sufficientlyVisible()
                      error:&error];
      return !error;
    };
    GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout,
                                           sideSwipeBubbleViewVisible),
               @"IPH not shown.");

    [[EarlGrey selectElementWithMatcher:SideSwipeBubble()]
        assertWithMatcher:grey_sufficientlyVisible()];
    ConditionBlock sideSwipeBubbleViewInisible = ^{
      NSError* error = nil;
      [[EarlGrey selectElementWithMatcher:Background()]
          assertWithMatcher:grey_nil()
                      error:&error];
      [[EarlGrey selectElementWithMatcher:SideSwipeBubble()]
          assertWithMatcher:grey_nil()
                      error:&error];
      return !error;
    };
    GREYAssert(
        WaitUntilConditionOrTimeout(timeout, sideSwipeBubbleViewInisible),
        @"IPH not auto dismissed.");
  }
  // Test that the correct dismissal reason is passed in the callback.
  [[EarlGrey selectElementWithMatcher:DismissedDueToTimeOut()]
      assertWithMatcher:grey_notNil()];
  Close();
}

@end

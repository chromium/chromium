// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_constants.h"
#import "ios/showcase/badges/sc_badge_constants.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::showcase_utils::Open;
using ::showcase_utils::Close;
}

// Tests for Badges.
@interface SCBadgeTestCase : ShowcaseTestCase
@end

@implementation SCBadgeTestCase

- (void)setUp {
  [super setUp];
  Open(@"Badge View");
}

- (void)tearDown {
  Close();
  [super tearDown];
}

// Tests that the passwords badge and incognito badge are displayed.
- (void)testBadgesVisible {
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kBadgeButtonSavePasswordAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on button to show the accepted badge.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSCShowAcceptedDisplayedBadgeButton)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(
                  kBadgeButtonSavePasswordAcceptedAccessibilityIdentifier),
              grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kBadgeButtonIncognitoAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the overflow badge presents and that the popup menu is presented
// when it is tapped.
- (void)testOverflowbadge {
  // Tap on button to show the overflow badge.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSCShowOverflowDisplayedBadgeButton)]
      performAction:grey_tap()];

  // Assert that overflow badge and the unread indicator is shown and tap on it.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kBadgeButtonOverflowAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kBadgeUnreadIndicatorAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBadgeButtonOverflowAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Assert that the badge overflow popup menu is being presented.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kBadgePopupMenuTableViewAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss popup menu by tapping outside of the menu. Tapping the displayed
  // badge is sufficient here. Assert that the unread indicator is not there
  // anymore.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBadgeButtonOverflowAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kBadgeUnreadIndicatorAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notVisible()];
}

@end

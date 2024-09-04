// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::ContextMenuItemWithAccessibilityLabelId;

namespace {

// Matcher for the feed header menu button.
id<GREYMatcher> FeedMenuButton() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_DISCOVER_FEED_MENU_ACCESSIBILITY_LABEL);
}
// Matcher for the Turn Off menu item in the feed menu.
id<GREYMatcher> TurnOffFeedMenuItem() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_DISCOVER_FEED_MENU_TURN_OFF_ITEM);
}
// Matcher for the Learn More menu item in the feed menu.
id<GREYMatcher> LearnMoreFeedMenuItem() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_DISCOVER_FEED_MENU_LEARN_MORE_ITEM);
}
// Matcher for the Manage menu item in the feed menu.
id<GREYMatcher> ManageFeedMenuItem() {
  return ContextMenuItemWithAccessibilityLabelId(
      IDS_IOS_DISCOVER_FEED_MENU_MANAGE_ITEM);
}

void SelectFeedMenu() {
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(FeedMenuButton(),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 100.0f)
      onElementWithMatcher:chrome_test_util::NTPCollectionView()]
      performAction:grey_tap()];
}

void SignInToFakeIdentity() {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:identity];
  [SigninEarlGrey signinWithFakeIdentity:identity];
  GREYWaitForAppToIdle(@"App failed to idle");

  // Close NTP and reopen. This is only needed for tests since the observer to
  // update the NTP after signing in doesn't work.
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
}

}  // namespace

@interface FeedManagementTestCase : ChromeTestCase
@end

@implementation FeedManagementTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kEnableWebChannels);
  config.features_disabled.push_back(kHomeCustomization);
  return config;
}

- (void)testSignedOutOpenAndCloseFeedMenu {
  SelectFeedMenu();

  [[EarlGrey selectElementWithMatcher:TurnOffFeedMenuItem()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:LearnMoreFeedMenuItem()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:ManageFeedMenuItem()]
      assertWithMatcher:grey_nil()];

  GREYAssertTrue([ChromeEarlGreyUI dismissContextMenuIfPresent],
                 @"Failed to dismiss context menu.");
}

- (void)testSignedInOpenAndCloseFeedMenu {
  SignInToFakeIdentity();
  SelectFeedMenu();

  [[EarlGrey selectElementWithMatcher:ManageFeedMenuItem()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TurnOffFeedMenuItem()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:LearnMoreFeedMenuItem()]
      assertWithMatcher:grey_notNil()];

  GREYAssertTrue([ChromeEarlGreyUI dismissContextMenuIfPresent],
                 @"Failed to dismiss context menu.");
}

- (void)testOpenFeedManagementSurface {
  SignInToFakeIdentity();
  SelectFeedMenu();

  [[EarlGrey selectElementWithMatcher:ManageFeedMenuItem()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
}

@end

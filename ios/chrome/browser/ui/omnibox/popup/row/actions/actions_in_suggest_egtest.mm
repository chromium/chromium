// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_earl_grey.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_test_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Unhighlighted call button matcher.
id<GREYMatcher> unhighlightedCallButtonMatcher() {
  return grey_accessibilityID(kCallActionIdentifier);
}

// Unhighlighted directions button matcher.
id<GREYMatcher> unhighlightedDirectionsButtonMatcher() {
  return grey_accessibilityID(kDirectionsActionIdentifier);
}

// Unhighlighted reviews button matcher.
id<GREYMatcher> unhighlightedReviewsButtonMatcher() {
  return grey_accessibilityID(kReviewsActionIdentifier);
}

// Unhighlighted call button matcher.
id<GREYMatcher> highlightedCallButtonMatcher() {
  return grey_accessibilityID(kCallActionHighlightedIdentifier);
}

// Unhighlighted call button matcher.
id<GREYMatcher> highlightedDirectionsButtonMatcher() {
  return grey_accessibilityID(kDirectionsActionHighlightedIdentifier);
}

// Unhighlighted call button matcher.
id<GREYMatcher> highlightedReviewsButtonMatcher() {
  return grey_accessibilityID(kReviewsActionHighlightedIdentifier);
}

}  // namespace

@interface OmniboxPopupWithFakeSuggestionActionsTestCase : ChromeTestCase
@end

@implementation OmniboxPopupWithFakeSuggestionActionsTestCase {
  GURL _directionURI;
  GURL _reviewsURI;
  GURL _callURI;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.features_disabled = {};
  config.features_enabled.push_back(kOmniboxActionsInSuggest);
  // HW keyboard simulation can mess up the SW keyboard simulator state.
  // Relaunching resets the state.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey clearBrowsingHistory];

  [OmniboxAppInterface
      setUpFakeSuggestionsService:@"fake_suggestion_actions.json"];
  [ChromeEarlGrey loadURL:GURL("about:blank")];

  _directionURI = GURL("http://localhost:3000/directions");
  _reviewsURI = GURL("http://localhost:3000/reviews");
  _callURI = GURL("tel://0123456789");
}

- (void)tearDown {
  [OmniboxAppInterface tearDownFakeSuggestionsService];
  [super tearDown];
}

- (void)testDisplayActions {
  // Clears the url and replace it with local url host.
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"local restaurant"];

  [self ensureActionButtonsAreDisplayed];
}

- (void)testTapDirectionsButton {
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"local restaurant"];

  [self ensureActionButtonsAreDisplayed];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      unhighlightedDirectionsButtonMatcher()];

  [[EarlGrey selectElementWithMatcher:unhighlightedDirectionsButtonMatcher()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::OmniboxText(
                                              _directionURI.GetContent())];
}

- (void)testTapReviewsButton {
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"local restaurant"];

  [self ensureActionButtonsAreDisplayed];

  [[EarlGrey selectElementWithMatcher:unhighlightedReviewsButtonMatcher()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::OmniboxText(
                                              _reviewsURI.GetContent())];
}

// TODO (crbug.com/348177731) re-enable when fixed.
- (void)DISABLED_testTapCallButton {
  // Skip the test if the dial app is not installed.
  if (![self dialAppInstalled]) {
    return;
  }

  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"local restaurant"];

  [self ensureActionButtonsAreDisplayed];

  [[EarlGrey selectElementWithMatcher:unhighlightedCallButtonMatcher()]
      performAction:grey_tap()];

  // The call dialog is a system UI so can't be tested using EG.
  XCUIApplication* springboardApplication = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.springboard"];

  // This phone number is hardcoded in the stubbed entity info.
  auto callLabel = springboardApplication.staticTexts[@"Call 01 23 45 67 89"];

  GREYAssert(
      [callLabel
          waitForExistenceWithTimeout:base::test::ios::kWaitForUIElementTimeout
                                          .InSecondsF()],
      @"Call dialog was not shown");

  if ([callLabel exists]) {
    auto cancelButton = springboardApplication.buttons[@"Cancel"];
    [cancelButton tap];
    // Make sure that the call dialog get discarded before the teardown.
    GREYAssertFalse(
        [callLabel waitForExistenceWithTimeout:
                       base::test::ios::kWaitForUIElementTimeout.InSecondsF()],
        @"Call dialog is still shown");
  }
}

- (void)testKeyboardArrowsHighlighting {
  BOOL isDialAppInstalled = [self dialAppInstalled];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"local restaurant"];

  [self ensureActionButtonsAreDisplayed];

  // Go down to the actions popup row.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"downArrow" flags:0];
  // Go down to the first action button.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"downArrow" flags:0];

  if (isDialAppInstalled) {
    // The call button should be highlighted.
    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:highlightedCallButtonMatcher()];
    // The directions and reviews buttons should stay unhighlighted.
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                        unhighlightedDirectionsButtonMatcher()];
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                        unhighlightedReviewsButtonMatcher()];
  } else {
    // The directions button should be highlighted.
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                        highlightedDirectionsButtonMatcher()];
    // The reviews button should stay unhighlighted.
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                        unhighlightedReviewsButtonMatcher()];
  }

  // Go to the next unhighlighted action button.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"rightArrow" flags:0];

  if (isDialAppInstalled) {
    // The directions button should now be highlighted.
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                        highlightedDirectionsButtonMatcher()];
    // The call button should now become unhighlighted
    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:unhighlightedCallButtonMatcher()];
  } else {
    // The reviews button should now be highlighted.
    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:highlightedReviewsButtonMatcher()];
    // The directions button should now become unhighlighted
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                        unhighlightedDirectionsButtonMatcher()];
  }

  // Go back to the previously highlighted action button.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"leftArrow" flags:0];

  if (isDialAppInstalled) {
    // The call button should be highlighted.
    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:highlightedCallButtonMatcher()];
    // The directions button should now become unhighlighted.
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                        unhighlightedDirectionsButtonMatcher()];
  } else {
    // The directions button should be highlighted.
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                        highlightedDirectionsButtonMatcher()];
    // The reviews button should now become unhighlighted.
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                        unhighlightedReviewsButtonMatcher()];
  }

  // Go up to unhighlight the action buttons
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"upArrow" flags:0];

  // Directions button should be displayed unhighlighted.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      unhighlightedDirectionsButtonMatcher()];
  // Reviews button should be displayed unhighlighted.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:unhighlightedReviewsButtonMatcher()];
  // Call button should be displayed only if the device has a dial app.
  if ([self dialAppInstalled]) {
    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:unhighlightedCallButtonMatcher()];
  } else {
    [[EarlGrey selectElementWithMatcher:unhighlightedCallButtonMatcher()]
        assertWithMatcher:grey_notVisible()];
  }
}

- (void)testReturnKeyOnDirectionsButton {
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"local restaurant"];

  [self ensureActionButtonsAreDisplayed];

  // Go down to the actions popup row.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"downArrow" flags:0];
  // Go down to the first action button.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"downArrow" flags:0];

  if ([self dialAppInstalled]) {
    // Highlight directions button.
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"rightArrow" flags:0];
  }

  // The directions button should be highlighted.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:highlightedDirectionsButtonMatcher()];

  // press the return key.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"return" flags:0];
  // We expect to trigger the reviews button action.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::OmniboxText(
                                              _directionURI.GetContent())];
}

- (void)testReturnKeyOnReviewsButton {
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"local restaurant"];

  [self ensureActionButtonsAreDisplayed];

  // Go down to the actions popup row.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"downArrow" flags:0];
  // Go down to the first action button.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"downArrow" flags:0];

  if ([self dialAppInstalled]) {
    // Highlight directions button.
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"rightArrow" flags:0];
    // Highlight reviews button.
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"rightArrow" flags:0];
  } else {
    // Highlight reviews button.
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"rightArrow" flags:0];
  }

  // The reviews button should be highlighted.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:highlightedReviewsButtonMatcher()];

  // press the return key.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"return" flags:0];
  // We expect to trigger the reviews button action.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::OmniboxText(
                                              _reviewsURI.GetContent())];
}

// TODO (crbug.com/348177731) re-enable when fixed.
- (void)DISABLED_testReturnKeyOnCallButton {
  // Skip the test if the dial app is not installed.
  if (![self dialAppInstalled]) {
    return;
  }

  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"local restaurant"];

  [self ensureActionButtonsAreDisplayed];

  // Go down to the actions popup row.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"downArrow" flags:0];
  // Go down to the call action button.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"downArrow" flags:0];

  // The call button should be highlighted.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:highlightedCallButtonMatcher()];

  // press the return key.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"return" flags:0];

  // The call dialog is a system UI so can't be tested using EG.
  XCUIApplication* springboardApplication = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.springboard"];

  // This phone number is hardcoded in the stubbed entity info.
  auto callLabel = springboardApplication.staticTexts[@"Call 01 23 45 67 89"];

  GREYAssert(
      [callLabel
          waitForExistenceWithTimeout:base::test::ios::kWaitForUIElementTimeout
                                          .InSecondsF()],
      @"Call dialog was not shown");

  if ([callLabel exists]) {
    auto cancelButton = springboardApplication.buttons[@"Cancel"];
    [cancelButton tap];
    // Make sure that the call dialog get discarded before the teardown.
    GREYAssertFalse(
        [callLabel waitForExistenceWithTimeout:
                       base::test::ios::kWaitForUIElementTimeout.InSecondsF()],
        @"Call dialog is still shown");
  }
}

#pragma mark - utils

// Ensures that the action buttons are displayed.
- (void)ensureActionButtonsAreDisplayed {
  // Directions button should be displayed unhighlighted.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      unhighlightedDirectionsButtonMatcher()];
  // Reviews button should be displayed unhighlighted.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:unhighlightedReviewsButtonMatcher()];
  // Call button should be displayed only if the device has a dial app.
  if ([self dialAppInstalled]) {
    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:unhighlightedCallButtonMatcher()];
  } else {
    [[EarlGrey selectElementWithMatcher:unhighlightedCallButtonMatcher()]
        assertWithMatcher:grey_notVisible()];
  }
}

- (BOOL)dialAppInstalled {
  return [[UIApplication sharedApplication]
      canOpenURL:net::NSURLWithGURL(_callURI)];
}

@end

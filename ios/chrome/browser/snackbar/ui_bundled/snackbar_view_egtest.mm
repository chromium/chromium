// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"
#import "ios/chrome/browser/snackbar/ui_bundled/snackbar_view_test_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

using chrome_test_util::SnackbarViewMatcher;

NSString* const kTestTitle = @"Test Title";
NSString* const kTestTitle2 = @"Second Test Title";
NSString* const kTestSubtitle = @"Test Subtitle";
NSString* const kTestSecondarySubtitle = @"Test Secondary Subtitle";
NSString* const kButtonTitle = @"Button Title";

// Returns a matcher for a snackbar element with the given `accessibility_id`.
id<GREYMatcher> SnackBarViewMatcherForAccessibilityId(
    NSString* accessibility_id) {
  return grey_allOf(grey_accessibilityID(accessibility_id),
                    grey_ancestor(SnackbarViewMatcher()), nil);
}

// Verifies the visibility of all snackbar components.
void VerifySnackbarUI(NSString* title,
                      NSString* subtitle,
                      NSString* secondarySubtitle,
                      NSString* buttonText,
                      BOOL hasLeadingAccessory,
                      BOOL hasTrailingAccessory) {
  [SnackbarViewTestAppInterface showSnackbarWithTitle:title
                                             subtitle:subtitle
                                    secondarySubtitle:secondarySubtitle
                                           buttonText:buttonText
                                  hasLeadingAccessory:hasLeadingAccessory
                                 hasTrailingAccessory:hasTrailingAccessory];
  [ChromeEarlGreyUI waitForAppToIdle];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SnackbarViewMatcher()];

  [[EarlGrey selectElementWithMatcher:SnackBarViewMatcherForAccessibilityId(
                                          kSnackbarTitleAccessibilityId)]
      assertWithMatcher:title ? grey_allOf(grey_text(title),
                                           grey_sufficientlyVisible(), nil)
                              : grey_nil()];
  [[EarlGrey selectElementWithMatcher:SnackBarViewMatcherForAccessibilityId(
                                          kSnackbarSubtitleAccessibilityId)]
      assertWithMatcher:subtitle ? grey_allOf(grey_text(subtitle),
                                              grey_sufficientlyVisible(), nil)
                                 : grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:SnackBarViewMatcherForAccessibilityId(
                                   kSnackbarSecondarySubtitleAccessibilityId)]
      assertWithMatcher:secondarySubtitle
                            ? grey_allOf(grey_text(secondarySubtitle),
                                         grey_sufficientlyVisible(), nil)
                            : grey_nil()];
  [[EarlGrey selectElementWithMatcher:SnackBarViewMatcherForAccessibilityId(
                                          kSnackbarButtonAccessibilityId)]
      assertWithMatcher:buttonText ? grey_allOf(grey_buttonTitle(buttonText),
                                                grey_sufficientlyVisible(), nil)
                                   : grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:SnackBarViewMatcherForAccessibilityId(
                                   kSnackbarLeadingAccessoryAccessibilityId)]
      assertWithMatcher:hasLeadingAccessory ? grey_sufficientlyVisible()
                                            : grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:SnackBarViewMatcherForAccessibilityId(
                                   kSnackbarTrailingAccessoryAccessibilityId)]
      assertWithMatcher:hasTrailingAccessory ? grey_sufficientlyVisible()
                                             : grey_nil()];
}

}  // namespace

// Test suite for SnackbarView.
@interface SnackbarViewTestCase : ChromeTestCase
@end

@implementation SnackbarViewTestCase

+ (BOOL)loadMinimalAppUI {
  return YES;
}

- (void)setUp {
  [super setUp];
  [ChromeCoordinatorAppInterface startSnackbarCoordinator];
}

- (void)tearDownHelper {
  [SnackbarViewTestAppInterface removeDummyTextField];
  [super tearDownHelper];
  [ChromeCoordinatorAppInterface reset];
}

// Tests a snackbar with only a title.
- (void)testSnackbarWithTitle {
  VerifySnackbarUI(kTestTitle, nil, nil, nil, NO, NO);
}

// Tests a snackbar with a title and a subtitle.
- (void)testSnackbarWithTitleAndSubtitle {
  VerifySnackbarUI(kTestTitle, kTestSubtitle, nil, nil, NO, NO);
}

// Tests a snackbar with a title, subtitle and secondary subtitle.
- (void)testSnackbarWithTitleSubtitleAndSecondarySubtitle {
  VerifySnackbarUI(kTestTitle, kTestSubtitle, kTestSecondarySubtitle, nil, NO,
                   NO);
}

// Tests a snackbar with an action button.
- (void)testSnackbarWithActionButton {
  VerifySnackbarUI(kTestTitle, nil, nil, kButtonTitle, NO, NO);
}

// Tests a snackbar with a leading accessory view.
- (void)testSnackbarWithLeadingAccessory {
  VerifySnackbarUI(kTestTitle, nil, nil, nil, YES, NO);
}

// Tests a snackbar with a trailing accessory view.
- (void)testSnackbarWithTrailingAccessory {
  VerifySnackbarUI(kTestTitle, nil, nil, nil, NO, YES);
}

// Tests that tapping the snackbar view dismisses it.
- (void)testSnackbarDismissesOnTap {
  VerifySnackbarUI(kTestTitle, nil, nil, nil, NO, NO);
  [[EarlGrey selectElementWithMatcher:SnackbarViewMatcher()]
      performAction:grey_tap()];

  // The snackbar should disappear after being tapped.
  [[EarlGrey selectElementWithMatcher:SnackbarViewMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that showing a new snackbar dismisses an existing one.
- (void)testNewSnackbarDismissesOldSnackbar {
  // Show the first message.
  VerifySnackbarUI(kTestTitle, nil, nil, nil, NO, NO);

  // Show the second snackbar.
  VerifySnackbarUI(kTestTitle2, nil, nil, nil, NO, NO);

  // Verify the first message is gone.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSnackbarTitleAccessibilityId),
                                          grey_text(kTestTitle), nil)]
      assertWithMatcher:grey_nil()];
}

// Tests that `showSnackbarMessageAfterDismissingKeyboard` displays the snackbar
// correctly after dismissing the keyboard.
- (void)testShowSnackbarMessageAfterDismissingKeyboard {
  // Ensure the dummy text field is in the view hierarchy.
  [SnackbarViewTestAppInterface makeTextFieldFirstResponder];
  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"dummyTextField")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"dummyTextField")]
      performAction:grey_tap()];

  // Verify keyboard is present.
  [ChromeEarlGrey waitForKeyboardToAppear];

  [SnackbarViewTestAppInterface
      showSnackbarMessageAfterDismissingKeyboardWithTitle:kTestTitle];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify keyboard is dismissed.
  [ChromeEarlGrey waitForKeyboardToDisappear];

  // Verify the snackbar is displayed correctly.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SnackbarViewMatcher()];
  [[EarlGrey selectElementWithMatcher:SnackBarViewMatcherForAccessibilityId(
                                          kSnackbarTitleAccessibilityId)]
      assertWithMatcher:grey_allOf(grey_text(kTestTitle),
                                   grey_sufficientlyVisible(), nil)];
}

@end

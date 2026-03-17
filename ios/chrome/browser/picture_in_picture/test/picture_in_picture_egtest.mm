// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/default_browser/promo/public/features.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Timeout for PiP window to appear.
const NSTimeInterval kPipAppearanceTimeout = 5.0;

// Timeout for PiP controls to appear.
const NSTimeInterval kPipControlsTimeout = 3.0;

// Accessibility label for the PiP close button.
NSString* const kPipCloseButtonLabel = @"Close Picture in Picture";

// Accessibility label for the PiP restore fullscreen button.
NSString* const kPipRestoreFullscreenButtonLabel = @"Restore fullscreen";

// Accessibility label for the SpringBoard breadcrumb button.
NSString* const kSpringBoardBreadcrumbLabel = @"breadcrumb";

// Identifier for the iOS SpringBoard process.
NSString* const kSpringBoardBundleIdentifier = @"com.apple.springboard";

// Accessibility identifier for the PiP window in SpringBoard.
NSString* const kPipWindowIdentifier = @"Picture in Picture";

// Navigates to the Default Browser Settings and triggers the PiP promo.
void OpenDefaultBrowserPictureInPicturePromo() {
  [ChromeEarlGreyUI openSettingsMenu];

  id<GREYMatcher> defaultBrowserRow = chrome_test_util::ContainsPartialText(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_DEFAULT_BROWSER));

  [[[EarlGrey selectElementWithMatcher:defaultBrowserRow]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      performAction:grey_tap()];

  id<GREYMatcher> primaryButton = chrome_test_util::ButtonStackPrimaryButton();
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:primaryButton];
  [[EarlGrey selectElementWithMatcher:primaryButton] performAction:grey_tap()];
}

// Returns a proxy to the iOS SpringBoard application. Picture in Picture is a
// system-level feature, so we need to interact with SpringBoard to control it.
XCUIApplication* SpringBoardApplication() {
  return [[XCUIApplication alloc]
      initWithBundleIdentifier:kSpringBoardBundleIdentifier];
}

// Returns the PiP window element within the given application.
XCUIElement* PipWindowInApplication(XCUIApplication* application) {
  return application.windows[kPipWindowIdentifier].firstMatch;
}

// Ensures that PiP controls are visible by tapping the PiP window if necessary.
void EnsurePipControlsAreVisibleForWindow(XCUIElement* pipWindow,
                                          XCUIElement* controlElement) {
  if (!controlElement.exists || !controlElement.isHittable) {
    [pipWindow tap];
  }
}

}  // namespace

@interface PictureInPictureTestCase : ChromeTestCase
@end

@implementation PictureInPictureTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kDefaultBrowserPictureInPicture);
  return config;
}

#pragma mark - Tests

// Tests that the PiP window for the Default Browser promo can be
// started and then dismissed using the close button.
- (void)testDefaultBrowserStartPipAndDismiss {
#if TARGET_OS_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"This test is not supported on simulator.");
#endif
  OpenDefaultBrowserPictureInPicturePromo();

  XCUIApplication* springboard = SpringBoardApplication();
  XCUIElement* pipWindow = PipWindowInApplication(springboard);

  GREYAssertTrue([pipWindow waitForExistenceWithTimeout:kPipAppearanceTimeout],
                 @"PiP window did not appear.");

  XCUIElement* closeButton = pipWindow.buttons[kPipCloseButtonLabel].firstMatch;

  // Ensure the close button is visible before tapping it.
  EnsurePipControlsAreVisibleForWindow(pipWindow, closeButton);

  BOOL successfullyClosed = NO;
  if ([closeButton waitForExistenceWithTimeout:kPipControlsTimeout]) {
    [closeButton tap];
    successfullyClosed = YES;
  }

  GREYAssertTrue(successfullyClosed,
                 @"Could not find or tap the close button.");

  // Verify that the Default Browser fullscreen video instructions title is not
  // visible.
  id<GREYMatcher> titleLabel =
      chrome_test_util::ContainsPartialText(l10n_util::GetNSString(
          IDS_IOS_DEFAULT_BROWSER_PICTURE_IN_PICTURE_TITLE_TEXT));
  [[EarlGrey selectElementWithMatcher:titleLabel]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the PiP window for the Default Browser promo can be
// started and then used to restore the app and display the video in fullscreen.
- (void)testDefaultBrowserStartPipAndRestoreFullscreen {
#if TARGET_OS_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"This test is not supported on simulator.");
#endif
  OpenDefaultBrowserPictureInPicturePromo();

  XCUIApplication* springboard = SpringBoardApplication();
  XCUIElement* pipWindow = PipWindowInApplication(springboard);

  GREYAssertTrue([pipWindow waitForExistenceWithTimeout:kPipAppearanceTimeout],
                 @"PiP window did not appear.");

  XCUIElement* restoreButton =
      pipWindow.buttons[kPipRestoreFullscreenButtonLabel].firstMatch;

  // Ensure the restore button is visible before tapping it.
  EnsurePipControlsAreVisibleForWindow(pipWindow, restoreButton);

  GREYAssertTrue(
      [restoreButton waitForExistenceWithTimeout:kPipControlsTimeout],
      @"Restore fullscreen button did not appear.");
  [restoreButton tap];

  // Verify that the Default Browser fullscreen video instructions title is
  // visible after restoration.
  id<GREYMatcher> titleLabel =
      chrome_test_util::ContainsPartialText(l10n_util::GetNSString(
          IDS_IOS_DEFAULT_BROWSER_PICTURE_IN_PICTURE_TITLE_TEXT));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:titleLabel];
}

// Tests that the PiP window for the Default Browser promo can be
// started and then the app can be restored via the status bar return to app
// button.
- (void)testDefaultBrowserStartPipAndRestoreViaReturnToApp {
#if TARGET_OS_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"This test is not supported on simulator.");
#endif
  OpenDefaultBrowserPictureInPicturePromo();

  XCUIApplication* springboard = SpringBoardApplication();
  XCUIElement* pipWindow = PipWindowInApplication(springboard);

  GREYAssertTrue([pipWindow waitForExistenceWithTimeout:kPipAppearanceTimeout],
                 @"PiP window did not appear.");

  XCUIElement* breadcrumbButton =
      springboard.statusBars.buttons[kSpringBoardBreadcrumbLabel].firstMatch;

  GREYAssertTrue(
      [breadcrumbButton waitForExistenceWithTimeout:kPipControlsTimeout],
      @"Status bar breadcrumb button did not appear.");
  [breadcrumbButton tap];

  // Verify that the Default Browser fullscreen video instructions title is not
  // visible after restoration.
  id<GREYMatcher> titleLabel =
      chrome_test_util::ContainsPartialText(l10n_util::GetNSString(
          IDS_IOS_DEFAULT_BROWSER_PICTURE_IN_PICTURE_TITLE_TEXT));
  [[EarlGrey selectElementWithMatcher:titleLabel]
      assertWithMatcher:grey_notVisible()];
}

@end

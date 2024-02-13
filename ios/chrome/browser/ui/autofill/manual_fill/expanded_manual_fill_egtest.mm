// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kFormPassword[] = "pw";

// Checks that the header view is as expected according to whether or not the
// device is in landscape mode.
void CheckHeader(bool is_landscape) {
  id<GREYMatcher> header_view =
      grey_accessibilityID(manual_fill::kExpandedManualFillHeaderViewID);
  [[EarlGrey selectElementWithMatcher:header_view]
      assertWithMatcher:grey_sufficientlyVisible()];

  // The header's top view should only be part of the UI when in portrait mode.
  id<GREYMatcher> header_top_view =
      grey_accessibilityID(manual_fill::kExpandedManualFillHeaderTopViewID);
  [[EarlGrey selectElementWithMatcher:header_top_view]
      assertWithMatcher:is_landscape ? grey_notVisible()
                                     : grey_sufficientlyVisible()];

  // Check Chrome logo and close button.
  id<GREYMatcher> chrome_logo;
  id<GREYMatcher> close_button;
  if (is_landscape) {
    chrome_logo =
        grey_accessibilityID(manual_fill::kExpandedManualFillChromeLogoID);
    close_button = grey_accessibilityLabel(l10n_util::GetNSString(
        IDS_IOS_EXPANDED_MANUAL_FILL_CLOSE_BUTTON_ACCESSIBILITY_LABEL));
  } else {
    // Chrome logo and close button should be placed inside the header's top
    // view in portrait mode.
    chrome_logo = grey_allOf(
        grey_accessibilityID(manual_fill::kExpandedManualFillChromeLogoID),
        grey_ancestor(header_top_view), nil);
    close_button = grey_allOf(
        grey_accessibilityLabel(l10n_util::GetNSString(
            IDS_IOS_EXPANDED_MANUAL_FILL_CLOSE_BUTTON_ACCESSIBILITY_LABEL)),
        grey_ancestor(header_top_view), nil);
  }

  [[EarlGrey selectElementWithMatcher:chrome_logo]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:close_button]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check data type tabs.
  id<GREYMatcher> password_tab = grey_accessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_PASSWORD_TAB_ACCESSIBILITY_LABEL));
  [[EarlGrey selectElementWithMatcher:password_tab]
      assertWithMatcher:grey_sufficientlyVisible()];

  id<GREYMatcher> payment_tab = grey_accessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_PAYMENT_TAB_ACCESSIBILITY_LABEL));
  [[EarlGrey selectElementWithMatcher:payment_tab]
      assertWithMatcher:grey_sufficientlyVisible()];

  id<GREYMatcher> address_tab = grey_accessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_ADDRESS_TAB_ACCESSIBILITY_LABEL));
  [[EarlGrey selectElementWithMatcher:address_tab]
      assertWithMatcher:grey_sufficientlyVisible()];
}

}  // namespace

// Test case for the expanded manual fill view.
@interface ExpandedManualFillTestCase : WebHttpServerChromeTestCase
@end

@implementation ExpandedManualFillTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  // Enable the Keyboard Accessory Upgrade feature.
  config.features_enabled.push_back(kIOSKeyboardAccessoryUpgrade);

  return config;
}

- (void)setUp {
  [super setUp];

  // The tested UI is not availble on iPad, so there's no need for any setup.
  if ([ChromeEarlGrey isIPadIdiom]) {
    return;
  }

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  [self openExpandedManualFillView];
}

// Opens the expanded manual fill view.
- (void)openExpandedManualFillView {
  // Load simple login form.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_login_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];

  // Tap on the password field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  // Open the expanded manual fill view.
  id<GREYMatcher> manualFillButton = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_AUTOFILL_DATA));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:manualFillButton];
  [[EarlGrey selectElementWithMatcher:manualFillButton]
      performAction:grey_tap()];

  // Confirm that the expanded manual fill view is visible.
  id<GREYMatcher> expandedManualFillView =
      grey_accessibilityID(manual_fill::kExpandedManualFillViewID);
  [[EarlGrey selectElementWithMatcher:expandedManualFillView]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Tests

// Tests that the expanded manual fill view header is correctly laid out
// according to the device's orientation.
- (void)testExpandedManualFillViewDeviceOrientation {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Expanded manual fill view is only available on iPhone.");
  }

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];
  CheckHeader(/*is_landscape=*/true);

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];
  CheckHeader(/*is_landscape=*/true);

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  CheckHeader(/*is_landscape=*/false);
}

@end

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_matchers.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using chrome_test_util::TapWebElementWithId;

namespace {

constexpr char kFormElementNormal[] = "normal_field";
constexpr char kFormElementReadonly[] = "readonly_field";

constexpr char kFormHTMLFile[] = "/readonly_form.html";

// Matcher for the address manual fill button.
id<GREYMatcher> KeyboardAccessoryAddressManualFill() {
  return [AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]
             ? grey_accessibilityLabel(l10n_util::GetNSString(
                   IDS_IOS_AUTOFILL_ADDRESS_AUTOFILL_DATA))
             : manual_fill::ProfilesIconMatcher();
}

// Matcher for the username chip button of a password option shown in the manual
// fallback.
id<GREYMatcher> UsernameChipButton() {
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabel(@"concrete username"),
      grey_interactable(), nullptr);
}

}  // namespace

// Integration Tests for fallback coordinator.
@interface FallbackViewControllerTestCase : ChromeTestCase
@end

@implementation FallbackViewControllerTestCase

- (void)setUp {
  [super setUp];
  [AutofillAppInterface clearProfilesStore];
  [AutofillAppInterface saveExampleProfile];

  [AutofillAppInterface clearProfilePasswordStore];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Hello"];
}

- (void)tearDown {
  [AutofillAppInterface clearProfilesStore];
  [AutofillAppInterface clearProfilePasswordStore];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isRunningTest:@selector
            (testPasswordsVisibleWhenOpenedFromNonPasswordField)]) {
    config.features_disabled.push_back(kIOSKeyboardAccessoryUpgrade);
  }

  return config;
}

// Tests that readonly fields don't have Manual Fallback icons.
- (void)testReadOnlyFieldDoesNotShowManualFallbackIcons {
  // Tap the readonly field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementReadonly)];

  // Verify that the address manual fill button is not visible.
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryAddressManualFill()]
      assertWithMatcher:grey_notVisible()];
}

// Tests the visibility of manual fill buttons when switching from a regular
// field to a read-only field.
- (void)testReadOnlyFieldDoesNotShowManualFallbackIconsAfterNormalField {
  // Tap the regular field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementNormal)];

  [ChromeEarlGrey waitForKeyboardToAppear];

  // Verify that the address manual fill button is visible.
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryAddressManualFill()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the readonly field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementReadonly)];

  // Verify that the address manual fill button is not visible.
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryAddressManualFill()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that normal fields have Manual Fallback icons after tapping a readonly
// field.
- (void)testNormalFieldHasManualFallbackIconsAfterReadonlyField {
  // Tap the readonly field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementReadonly)];

  // Verify that the address manual fill button is not visible.
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryAddressManualFill()]
      assertWithMatcher:grey_notVisible()];

  // Tap the regular field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementNormal)];

  // Verify that the address manual fill button is visible.
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryAddressManualFill()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that saved passwords for the current site are visible in the manual
// fallback even when the focused field is not password-related.
- (void)testPasswordsVisibleWhenOpenedFromNonPasswordField {
  // Save a password for the current site.
  NSString* URLString =
      base::SysUTF8ToNSString(self.testServer->GetURL(kFormHTMLFile).spec());
  [AutofillAppInterface savePasswordFormForURLSpec:URLString];

  // Tap the regular field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementNormal)];

  // Tap the password icon to open manual fallback.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordIconMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Confirm that the password option is visible.
  [[EarlGrey selectElementWithMatcher:UsernameChipButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end

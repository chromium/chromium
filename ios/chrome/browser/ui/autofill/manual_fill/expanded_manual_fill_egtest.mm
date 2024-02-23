// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"

using manual_fill::ManualFillDataType;
using net::test_server::EmbeddedTestServer;

namespace {
constexpr char kAddressFormURL[] = "/profile_form.html";
constexpr char kPaymentMethodFormURL[] = "/credit_card.html";
constexpr char kPasswordFormURL[] = "/simple_login_form.html";

const char kCardNameFieldID[] = "CCName";
const char kPasswordFieldID[] = "pw";
const char kNameFieldID[] = "name";

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

// Loads a form depending on the desired `data_type`.
void LoadForm(EmbeddedTestServer* test_server, ManualFillDataType data_type) {
  base::StringPiece form_url;
  std::string form_text;
  switch (data_type) {
    case ManualFillDataType::kPassword:
      form_url = kPasswordFormURL;
      form_text = "Login form.";
      break;
    case ManualFillDataType::kPaymentMethod:
      form_url = kPaymentMethodFormURL;
      form_text = "Autofill Test";
      break;
    case ManualFillDataType::kAddress:
      form_url = kAddressFormURL;
      form_text = "Profile form";
      break;
  }

  [ChromeEarlGrey loadURL:test_server->GetURL(form_url)];
  [ChromeEarlGrey waitForWebStateContainingText:form_text];
}

// Saves a password for the login form.
void SavePasswordForLoginForm(EmbeddedTestServer* test_server) {
  [AutofillAppInterface
      savePasswordFormForURLSpec:base::SysUTF8ToNSString(
                                     test_server->GetURL(kPasswordFormURL)
                                         .spec())];
}

// Dismisses the payment bottom sheet by tapping the "Use Keyboard" button.
void DismissPaymentBottomSheet() {
  id<GREYMatcher> useKeyboardButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_PAYMENT_BOTTOM_SHEET_USE_KEYBOARD);
  [[EarlGrey selectElementWithMatcher:useKeyboardButton]
      performAction:grey_tap()];
}

// Makes sure that the payment suggestions are appearing in the keyboard
// accessory.
void MakeSurePaymentMethodSuggestionsAreVisisble() {
  // Needed in order to see the payment method suggestions.
  [AutofillAppInterface considerCreditCardFormSecureForTesting];

  DismissPaymentBottomSheet();

  // Wait for the payment suggestions to appear after dismissing the payment
  // bottom sheet.
  autofill::CreditCard card = autofill::test::GetCreditCard();
  id<GREYMatcher> cc_chip = grey_text(base::SysUTF16ToNSString(card.GetInfo(
      autofill::CREDIT_CARD_NAME_FULL, l10n_util::GetLocaleOverride())));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:cc_chip];
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

  // Enable the Password Suggestion Bottom Sheet.
  config.features_enabled.push_back(
      password_manager::features::kIOSPasswordBottomSheet);

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

  // Save a password, credit card and address.
  SavePasswordForLoginForm(self.testServer);
  [AutofillAppInterface saveLocalCreditCard];
  [AutofillAppInterface saveExampleAccountProfile];

  // Set password bottom sheet dismiss count so that the sheet won't be
  // presented.
  [PasswordSuggestionBottomSheetAppInterface setDismissCount:3];
}

- (void)tearDown {
  [super tearDown];

  // Clear the stores.
  [AutofillAppInterface clearProfilePasswordStore];
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface clearProfilesStore];
}

// Opens the expanded manual fill view for a given `dataType`. `fieldToFill` is
// the ID of the form field that should be focused prior to opening the expanded
// manual fill view.
- (void)openExpandedManualFillViewForDataType:(ManualFillDataType)dataType
                                  fieldToFill:(std::string)fieldToFill {
  // Load form.
  LoadForm(self.testServer, dataType);

  // Tap on the provided field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(fieldToFill)];

  if (dataType == ManualFillDataType::kPaymentMethod) {
    MakeSurePaymentMethodSuggestionsAreVisisble();
  }

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

  [self openExpandedManualFillViewForDataType:ManualFillDataType::kPassword
                                  fieldToFill:kPasswordFieldID];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];
  CheckHeader(/*is_landscape=*/true);

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                error:nil];
  CheckHeader(/*is_landscape=*/true);

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  CheckHeader(/*is_landscape=*/false);
}

// Tests that password manual filling options are visible when the expanded
// manual fill view is opened from a field for which there are password
// suggestions.
- (void)testOpeningExpandedManualFillViewForPassword {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Expanded manual fill view is only available on iPhone.");
  }

  // Open the expanded manual fill view for a password field.
  [self openExpandedManualFillViewForDataType:ManualFillDataType::kPassword
                                  fieldToFill:kPasswordFieldID];

  // The password view controller should be visible.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ManualFallbackPasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that payment method manual filling options are visible when the
// expanded manual fill view is opened from a field for which there are payment
// method suggestions.
- (void)testOpeningExpandedManualFillViewForPaymentMethod {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Expanded manual fill view is only available on iPhone.");
  }

  // Open the expanded manual fill view for a payment method field.
  [self openExpandedManualFillViewForDataType:ManualFillDataType::kPaymentMethod
                                  fieldToFill:kCardNameFieldID];

  // The payment method view controller should be visible.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ManualFallbackCreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that address manual filling options are visible when the expanded
// manual fill view is opened from a field for which there are address
// suggestions.
- (void)testOpeningExpandedManualFillViewForAddress {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Expanded manual fill view is only available on iPhone.");
  }

  // Open the expanded manual fill view for an address field.
  [self openExpandedManualFillViewForDataType:ManualFillDataType::kAddress
                                  fieldToFill:kNameFieldID];

  // The address view controller should be visible.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end

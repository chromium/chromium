// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/sync/service/sync_prefs.h"
#import "ios/chrome/browser/passwords/model/password_manager_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_app_interface.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_app_interface.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

constexpr char kFormUsername[] = "un";
constexpr char kFormPassword[] = "pw";

constexpr char kSigninUffFormUsername[] = "single_un";
constexpr char kSigninUffFormPassword[] = "single_pw";

constexpr char kFormCardName[] = "CCName";
constexpr char kFormCardNumber[] = "CCNo";
constexpr char kFormCardExpirationMonth[] = "CCExpiresMonth";
constexpr char kFormCardExpirationYear[] = "CCExpiresYear";

constexpr char kFormName[] = "form_name";
constexpr char kFormAddress[] = "form_address";
constexpr char kFormCity[] = "form_city";
constexpr char kFormState[] = "form_state";
constexpr char kFormZip[] = "form_zip";

@interface FormInputAccessoryEGTest : WebHttpServerChromeTestCase
@end

@implementation FormInputAccessoryEGTest

- (void)setUp {
  [super setUp];

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  // Make sure a credit card suggestion is available.
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface saveLocalCreditCard];
  // Make sure an address suggestion is available.
  [AutofillAppInterface clearProfilesStore];
  [AutofillAppInterface saveExampleProfile];
}

- (void)tearDown {
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface clearProfilesStore];
  GREYAssertTrue([PasswordManagerAppInterface clearCredentials],
                 @"Clearing credentials wasn't done.");
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(
      autofill::features::test::kAutofillServerCommunication);

  if ([self isRunningTest:@selector(testFillFieldOnFormWithSingleUsername)] ||
      [self isRunningTest:@selector(testFillFieldOnFormWithSinglePassword)]) {
    config.features_enabled.push_back(
        password_manager::features::kIOSPasswordSignInUff);
  } else {
    config.features_disabled.push_back(
        password_manager::features::kIOSPasswordSignInUff);
  }
  if ([self isRunningTest:@selector(testOpenExpandedManualFillView)]) {
    config.features_enabled.push_back(kIOSKeyboardAccessoryUpgrade);
  }
  return config;
}

#pragma mark - Helper methods

// Loads simple login page on localhost.
- (void)loadLoginPage {
  // Loads simple page.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_login_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];
}

// Load page on localhost to test username first flows.
- (void)loadUffLoginPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/uff_login_forms.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Single username form."];
}

// Loads simple credit card page on localhost.
- (void)loadPaymentsPage {
  // Loads simple page.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/credit_card.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Autofill Test"];

  // Localhost is not considered secure, therefore form security needs to be
  // overridden for the tests to work. This will allow us to fill the textfields
  // on the web page.
  [AutofillAppInterface considerCreditCardFormSecureForTesting];
}

// Loads simple address page on localhost.
- (void)loadAddressPage {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/autofill_smoke_test.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile Autofill"];
}

// Verifies that field with the html `id` has been filled with `value`.
- (void)verifyFieldWithIdHasBeenFilled:(std::string)id value:(NSString*)value {
  NSString* condition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       id.c_str(), value];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

// Verifies that the username and password fields are filled.
- (void)verifyFieldsHaveBeenFilledWithUsername:(NSString*)username
                                      password:(NSString*)password {
  // Verify that the username field has been filled.
  [self verifyFieldWithIdHasBeenFilled:kFormUsername value:username];

  // Verify that the password field has been filled.
  [self verifyFieldWithIdHasBeenFilled:kFormPassword value:password];
}

// Verify credit card infos are filled.
- (void)verifyCreditCardInfosHaveBeenFilled:(autofill::CreditCard)card {
  std::string locale = l10n_util::GetLocaleOverride();
  // Credit card name.
  NSString* name = base::SysUTF16ToNSString(
      card.GetInfo(autofill::CREDIT_CARD_NAME_FULL, locale));
  [self verifyFieldWithIdHasBeenFilled:kFormCardName value:name];

  // Credit card number.
  NSString* number = base::SysUTF16ToNSString(
      card.GetInfo(autofill::CREDIT_CARD_NUMBER, locale));
  [self verifyFieldWithIdHasBeenFilled:kFormCardNumber value:number];

  // Credit card expiration month.
  NSString* expMonth = base::SysUTF16ToNSString(
      card.GetInfo(autofill::CREDIT_CARD_EXP_MONTH, locale));
  [self verifyFieldWithIdHasBeenFilled:kFormCardExpirationMonth value:expMonth];

  // Credit card expiration year.
  NSString* expYear = base::SysUTF16ToNSString(
      card.GetInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR, locale));
  [self verifyFieldWithIdHasBeenFilled:kFormCardExpirationYear value:expYear];
}

// Verify address infos are filled.
- (void)verifyAddressInfosHaveBeenFilled:(autofill::AutofillProfile)profile {
  std::string locale = l10n_util::GetLocaleOverride();
  // Address name.
  NSString* name =
      base::SysUTF16ToNSString(profile.GetInfo(autofill::NAME_FULL, locale));
  [self verifyFieldWithIdHasBeenFilled:kFormName value:name];

  // Street address.
  NSString* address = base::SysUTF16ToNSString(
      profile.GetInfo(autofill::ADDRESS_HOME_LINE1, locale));
  [self verifyFieldWithIdHasBeenFilled:kFormAddress value:address];

  // Address City.
  NSString* city = base::SysUTF16ToNSString(
      profile.GetInfo(autofill::ADDRESS_HOME_CITY, locale));
  [self verifyFieldWithIdHasBeenFilled:kFormCity value:city];

  // Address State.
  NSString* state = base::SysUTF16ToNSString(
      profile.GetInfo(autofill::ADDRESS_HOME_STATE, locale));
  [self verifyFieldWithIdHasBeenFilled:kFormState value:state];

  // Address Zip
  NSString* zip = base::SysUTF16ToNSString(
      profile.GetInfo(autofill::ADDRESS_HOME_ZIP, locale));
  [self verifyFieldWithIdHasBeenFilled:kFormZip value:zip];
}

// Matcher for the bottom sheet's "Use Keyboard" button.
id<GREYMatcher> PaymentsBottomSheetUseKeyboardButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_PAYMENT_BOTTOM_SHEET_USE_KEYBOARD);
}

#pragma mark - Tests

// Tests that tapping on a password related field opens the keyboard accessory
// with the proper suggestion visible and that tapping on that suggestion
// properly fills the related fields on the form.
- (void)testFillPasswordFieldsOnForm {
  // Disable the password bottom sheet.
  [PasswordSuggestionBottomSheetAppInterface disableBottomSheet];

  [FormInputAccessoryAppInterface setUpMockReauthenticationModule];
  [FormInputAccessoryAppInterface mockReauthenticationModuleExpectedResult:
                                      ReauthenticationResult::kSuccess];

  NSString* username = @"user";
  NSString* password = @"password";
  [PasswordManagerAppInterface
      storeCredentialWithUsername:username
                         password:password
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form.html"))];
  [self loadLoginPage];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  id<GREYMatcher> user_chip = grey_accessibilityLabel(@"user ••••••••");

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:user_chip];

  [[EarlGrey selectElementWithMatcher:user_chip] performAction:grey_tap()];

  [self verifyFieldsHaveBeenFilledWithUsername:username password:password];

  [FormInputAccessoryAppInterface removeMockReauthenticationModule];
}

// Tests that the username field is filled when it is the only field in the
// sign-in form.
- (void)testFillFieldOnFormWithSingleUsername {
  // Disable the password bottom sheet.
  [PasswordSuggestionBottomSheetAppInterface disableBottomSheet];

  [FormInputAccessoryAppInterface setUpMockReauthenticationModule];
  [FormInputAccessoryAppInterface mockReauthenticationModuleExpectedResult:
                                      ReauthenticationResult::kSuccess];

  NSString* username = @"user";
  NSString* password = @"password";
  [PasswordManagerAppInterface
      storeCredentialWithUsername:username
                         password:password
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/uff_login_forms.html"))];
  [self loadUffLoginPage];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(
                        kSigninUffFormUsername)];

  id<GREYMatcher> user_chip = grey_accessibilityLabel(@"user ••••••••");

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:user_chip];

  [[EarlGrey selectElementWithMatcher:user_chip] performAction:grey_tap()];

  [self verifyFieldWithIdHasBeenFilled:kSigninUffFormUsername value:username];

  [FormInputAccessoryAppInterface removeMockReauthenticationModule];
}

// Tests that the password field is filled when it is the only field in the
// sign-in form.
- (void)testFillFieldOnFormWithSinglePassword {
  // Disable the password bottom sheet.
  [PasswordSuggestionBottomSheetAppInterface disableBottomSheet];

  [FormInputAccessoryAppInterface setUpMockReauthenticationModule];
  [FormInputAccessoryAppInterface mockReauthenticationModuleExpectedResult:
                                      ReauthenticationResult::kSuccess];

  NSString* username = @"user";
  NSString* password = @"password";
  [PasswordManagerAppInterface
      storeCredentialWithUsername:username
                         password:password
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/uff_login_forms.html"))];
  [self loadUffLoginPage];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(
                        kSigninUffFormPassword)];

  id<GREYMatcher> user_chip = grey_accessibilityLabel(@"user ••••••••");

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:user_chip];

  [[EarlGrey selectElementWithMatcher:user_chip] performAction:grey_tap()];

  [self verifyFieldWithIdHasBeenFilled:kSigninUffFormPassword value:password];

  [FormInputAccessoryAppInterface removeMockReauthenticationModule];
}

// Tests that tapping on a credit card related field opens the keyboard
// accessory with the proper suggestion visible and that tapping on that
// suggestion properly fills the related fields on the form.
- (void)testFillCreditCardFieldsOnForm {
  [AutofillAppInterface setUpMockReauthenticationModule];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];

  [self loadPaymentsPage];

  // Tap a credit card field to open the bottom sheet.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  id<GREYMatcher> useKeyboardButton = PaymentsBottomSheetUseKeyboardButton();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:useKeyboardButton];

  // Dismiss the bottom sheet and open the keyboard.
  [[EarlGrey selectElementWithMatcher:useKeyboardButton]
      performAction:grey_tap()];

  autofill::CreditCard card = autofill::test::GetCreditCard();

  id<GREYMatcher> cc_chip = grey_text(base::SysUTF16ToNSString(card.GetInfo(
      autofill::CREDIT_CARD_NAME_FULL, l10n_util::GetLocaleOverride())));

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:cc_chip];

  [[EarlGrey selectElementWithMatcher:cc_chip] performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self verifyCreditCardInfosHaveBeenFilled:card];

  [AutofillAppInterface clearMockReauthenticationModule];
}

// Tests that tapping on an address related field opens the keyboard
// accessory with the proper suggestion visible and that tapping on that
// suggestion properly fills the related fields on the form.
- (void)testFillAddressFieldsOnForm {
  [self loadAddressPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormZip)];

  autofill::AutofillProfile profile = autofill::test::GetFullProfile();

  id<GREYMatcher> address_chip = grey_text(
      base::SysUTF16ToNSString(profile.GetRawInfo(autofill::ADDRESS_HOME_ZIP)));

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:address_chip];

  [[EarlGrey selectElementWithMatcher:address_chip] performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self verifyAddressInfosHaveBeenFilled:profile];
}

// Tests that the manual fill button opens the expanded manual fill view.
- (void)testOpenExpandedManualFillView {
  // The expanded manual fill view UI is not available on tablets.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test not supported on iPad");
  }

  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  id<GREYMatcher> manual_fill_button = grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_AUTOFILL_PASSWORD_AUTOFILL_DATA)),
      grey_ancestor(
          grey_accessibilityID(kFormInputAccessoryViewAccessibilityID)),
      nil);

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:manual_fill_button];

  [[EarlGrey selectElementWithMatcher:manual_fill_button]
      performAction:grey_tap()];

  id<GREYMatcher> expanded_manual_fill_view =
      grey_accessibilityID(manual_fill::kExpandedManualFillViewID);

  [[EarlGrey selectElementWithMatcher:expanded_manual_fill_view]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end

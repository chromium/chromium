// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import <tuple>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/sync/service/sync_prefs.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_matchers.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/passwords/model/password_manager_app_interface.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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

namespace {

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

constexpr NSString* kExampleUsername = @"user";

// Matcher for the autofill password suggestion chip in the keyboard accessory.
id<GREYMatcher> KeyboardAccessoryPasswordSuggestion() {
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    return grey_allOf(grey_text(kExampleUsername),
                      grey_ancestor(grey_accessibilityID(
                          kFormInputAccessoryViewAccessibilityID)),
                      nil);
  }

  return grey_accessibilityLabel(
      [NSString stringWithFormat:@"%@ ••••••••", kExampleUsername]);
}

// Verifies that the number of accepted address suggestions recorded for the
// given `suggestion_index` is as expected.
void CheckAddressAutofillSuggestionAcceptedIndexMetricsCount(
    NSInteger suggestion_index) {
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:suggestion_index
                         forHistogram:
                             @"Autofill.SuggestionAcceptedIndex.Profile"],
      @"Unexpected histogram count for accepted address suggestion index.");

  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:suggestion_index
                         forHistogram:@"Autofill.UserAcceptedSuggestionAtIndex."
                                      @"Address.KeyboardAccessory"],
      @"Unexpected histogram count for keyboard accessory accepted address "
      @"suggestion index.");
}

// Verifies that the number of accepted card suggestions recorded for the given
// `suggestion_index` is as expected.
void CheckCardAutofillSuggestionAcceptedIndexMetricsCount(
    NSInteger suggestion_index) {
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:suggestion_index
                         forHistogram:
                             @"Autofill.SuggestionAcceptedIndex.CreditCard"],
      @"Unexpected histogram count for accepted card suggestion index.");

  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:suggestion_index
                         forHistogram:@"Autofill.UserAcceptedSuggestionAtIndex."
                                      @"CreditCard.KeyboardAccessory"],
      @"Unexpected histogram count for keyboard accessory accepted card "
      @"suggestion index.");
}

// Verifies that the number of accepted password suggestions recorded for the
// given `suggestion_index` is as expected.
void CheckPasswordAutofillSuggestionAcceptedIndexMetricsCount(
    NSInteger suggestion_index) {
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:suggestion_index
                         forHistogram:@"Autofill.UserAcceptedSuggestionAtIndex."
                                      @"Password.KeyboardAccessory"],
      @"Unexpected histogram count for keyboard accessory accepted password "
      @"suggestion index.");
}

}  // namespace

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

  // Set up histogram tester.
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
}

- (void)tearDown {
  // Close tabs  before clearing the stores in the case the
  // stores are needed when the tabs are closing (e.g. to upload votes).
  [[self class] closeAllTabs];

  GREYAssertTrue([PasswordManagerAppInterface clearCredentials],
                 @"Clearing credentials wasn't done.");
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface clearProfilesStore];

  // Clean up histogram tester.
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Failed to release histogram tester.");
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(
      autofill::features::test::kAutofillServerCommunication);
  if ([self isRunningTest:@selector(testOpenExpandedManualFillView)]) {
    config.features_enabled.push_back(kIOSKeyboardAccessoryUpgrade);
  }
  if ([self isRunningTest:@selector(testFillXframeCreditCardForm)]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillAcrossIframesIos);
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

// Loads a page with a xframe credit card form hosted on localhost.
- (void)loadXframePaymentPage {
  // Loads page with xframe credit card from.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/xframe_credit_card.html")];
  [ChromeEarlGrey
      waitForWebStateContainingText:"Autofill Test - Xframe Credit Card"];

  // Allow filling credit card data on the non-https localhost.
  [AutofillAppInterface considerCreditCardFormSecureForTesting];
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

// Verifies that html field with the `id_attr` attribute has been filled with
// `value`.
- (void)verifyFieldWithIdHasBeenFilled:(std::string)id_attr
                                 value:(NSString*)value {
  [self verifyFieldWithIdHasBeenFilled:id_attr iframeId:"" value:value];
}

// Verifies that the field with the `id_attr` attribute in the frame with the
// `frameId` attribute is filled with `value`. `id_attr` correspond to the HTML
// element id attribute. Verifies the main frame when `frameId` is empty.
// Verfies child frames of the main frame when `frameId` is not empty.
- (void)verifyFieldWithIdHasBeenFilled:(std::string)id_attr
                              iframeId:(std::string)frameId
                                 value:(NSString*)value {
  NSString* condition =
      frameId.empty()
          ? [NSString
                stringWithFormat:
                    @"window.document.getElementById('%s').value === '%@'",
                    id_attr.c_str(), value]
          : [NSString stringWithFormat:
                          @"document.getElementById('%s').contentDocument."
                          @"getElementById('%s').value === '%@'",
                          frameId.c_str(), id_attr.c_str(), value];
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

  NSString* username = kExampleUsername;
  NSString* password = @"password";
  [PasswordManagerAppInterface
      storeCredentialWithUsername:username
                         password:password
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form.html"))];
  [self loadLoginPage];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  id<GREYMatcher> user_chip = KeyboardAccessoryPasswordSuggestion();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:user_chip];

  [[EarlGrey selectElementWithMatcher:user_chip] performAction:grey_tap()];

  [self verifyFieldsHaveBeenFilledWithUsername:username password:password];

  // Verify that the acceptance of the password suggestion at index 0 was
  // correctly recorded.
  CheckPasswordAutofillSuggestionAcceptedIndexMetricsCount(
      /*suggestion_index=*/0);

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

  NSString* username = kExampleUsername;
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

  id<GREYMatcher> user_chip = KeyboardAccessoryPasswordSuggestion();

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

  NSString* username = kExampleUsername;
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

  id<GREYMatcher> user_chip = KeyboardAccessoryPasswordSuggestion();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:user_chip];

  [[EarlGrey selectElementWithMatcher:user_chip] performAction:grey_tap()];

  [self verifyFieldWithIdHasBeenFilled:kSigninUffFormPassword value:password];

  [FormInputAccessoryAppInterface removeMockReauthenticationModule];
}

// Tests that tapping on a credit card related field opens the keyboard
// accessory with the proper suggestions visible and that tapping on a
// suggestion properly fills the related fields on the form.
- (void)testFillCreditCardFieldsOnForm {
  [AutofillAppInterface setUpMockReauthenticationModule];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];

  [AutofillAppInterface saveMaskedCreditCard];

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

  // Wait for the keyboard accessory to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      manual_fill::FormSuggestionViewMatcher()];

  // Scroll to the right of the keyboard accessory so that the second card
  // suggestion is visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];

  id<GREYMatcher> cc_chip = grey_text(base::SysUTF16ToNSString(card.GetInfo(
      autofill::CREDIT_CARD_NAME_FULL, l10n_util::GetLocaleOverride())));
  [[EarlGrey selectElementWithMatcher:cc_chip] performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self verifyCreditCardInfosHaveBeenFilled:card];

  // Verify that the acceptance of the card suggestion at index 1 was correctly
  // recorded.
  CheckCardAutofillSuggestionAcceptedIndexMetricsCount(/*suggestion_index=*/1);

  [AutofillAppInterface clearMockReauthenticationModule];
}

// Tests that a xframe credit card form can be filled from the keyboard
// accessory.
- (void)testFillXframeCreditCardForm {
  // Mock reauth so it allows filling sensitive information without the need for
  // real authentication.
  [AutofillAppInterface setUpMockReauthenticationModule];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];

  // Load the xframe payment page.
  [self loadXframePaymentPage];

  // Tap a credit card field to open the bottom sheet.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];
  id<GREYMatcher> useKeyboardButton = PaymentsBottomSheetUseKeyboardButton();
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:useKeyboardButton];

  // Dismiss the bottom sheet and open the keyboard.
  [[EarlGrey selectElementWithMatcher:useKeyboardButton]
      performAction:grey_tap()];

  autofill::CreditCard card = autofill::test::GetCreditCard();

  // Tap on the credit card chip.
  id<GREYMatcher> cc_chip = grey_text(base::SysUTF16ToNSString(card.GetInfo(
      autofill::CREDIT_CARD_NAME_FULL, l10n_util::GetLocaleOverride())));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:cc_chip];
  [[EarlGrey selectElementWithMatcher:cc_chip] performAction:grey_tap()];

  // Verify that the credit card fields were filled correctly across frames.
  std::string locale = l10n_util::GetLocaleOverride();
  std::vector<std::tuple<autofill::FieldType, std::string, std::string>>
      fields_to_verify = {
          std::make_tuple(autofill::CREDIT_CARD_NAME_FULL, "", kFormCardName),
          std::make_tuple(autofill::CREDIT_CARD_NUMBER, "cc-number-frame",
                          kFormCardNumber),
          std::make_tuple(autofill::CREDIT_CARD_EXP_MONTH, "cc-exp-frame",
                          kFormCardExpirationMonth),
          std::make_tuple(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
                          "cc-exp-frame", kFormCardExpirationYear),
  };
  for (const auto& [field_type, frame_id_attr, field_id_attr] :
       fields_to_verify) {
    NSString* value =
        base::SysUTF16ToNSString(card.GetInfo(field_type, locale));
    [self verifyFieldWithIdHasBeenFilled:field_id_attr
                                iframeId:frame_id_attr
                                   value:value];
  }

  // Cleanup.
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

  // Verify that the acceptance of the address suggestion at index 0 was
  // correctly recorded.
  CheckAddressAutofillSuggestionAcceptedIndexMetricsCount(
      /*suggestion_index=*/0);
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

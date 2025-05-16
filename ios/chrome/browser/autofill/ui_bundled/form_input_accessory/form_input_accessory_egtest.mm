// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import <tuple>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/features.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/strings/grit/components_strings.h"
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
id<GREYMatcher> KeyboardAccessoryPasswordSuggestion(
    net::EmbeddedTestServer* test_server) {
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    NSString* chip_text = kExampleUsername;
    if ([ChromeEarlGrey isIPadIdiom]) {
      // On iPad, the suggestion text is an attributed string containing the
      // signon realm on the 2nd line.
      NSString* realm =
          base::SysUTF8ToNSString(password_manager::GetShownOrigin(
              url::Origin::Create(test_server->base_url())));
      chip_text = [NSString stringWithFormat:@"%@\n%@", chip_text, realm];
    }
    return grey_allOf(grey_text(chip_text),
                      grey_ancestor(grey_accessibilityID(
                          kFormInputAccessoryViewAccessibilityID)),
                      nil);
  }

  return grey_accessibilityLabel(
      [NSString stringWithFormat:@"%@ ••••••••", kExampleUsername]);
}

// Matcher for the credit card suggestion chip.
id<GREYMatcher> KeyboardAccessoryCreditCardSuggestion() {
  autofill::CreditCard card = autofill::test::GetCreditCard();

  NSString* username = base::SysUTF16ToNSString(card.GetInfo(
      autofill::CREDIT_CARD_NAME_FULL, l10n_util::GetLocaleOverride()));
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled] &&
      [ChromeEarlGrey isIPadIdiom]) {
    // On iPad, the suggestion text is an attributed string containing the
    // obfuscated credit card on the 2nd line.
    NSString* network = base::SysUTF16ToNSString(
        card.NetworkAndLastFourDigits(/*obfuscation_length=*/2));
    return grey_text([NSString stringWithFormat:@"%@\n%@", username, network]);
  } else {
    return grey_text(username);
  }
}

// Matcher for the address suggestion chip.
id<GREYMatcher> KeyboardAccessoryAddressSuggestion(
    autofill::FieldType field_type) {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  NSString* value = base::SysUTF16ToNSString(profile.GetRawInfo(field_type));
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled] &&
      [ChromeEarlGrey isIPadIdiom]) {
    // On iPad, the suggestion text is an attributed string containing the
    // street address on the 2nd line.
    NSString* street_address = base::SysUTF16ToNSString(
        profile.GetRawInfo(autofill::ADDRESS_HOME_LINE1));
    return grey_text(
        [NSString stringWithFormat:@"%@\n%@", value, street_address]);
  } else {
    return grey_text(value);
  }
}

// Matcher for the name suggestion chip.
id<GREYMatcher> KeyboardAccessoryNameSuggestion() {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  NSString* name =
      base::SysUTF16ToNSString(profile.GetRawInfo(autofill::NAME_FULL));
  if ([AutofillAppInterface isKeyboardAccessoryUpgradeEnabled] &&
      [ChromeEarlGrey isIPadIdiom]) {
    // On iPad, the suggestion text is an attributed string containing the state
    // on the 2nd line.
    NSString* state = base::SysUTF16ToNSString(
        profile.GetRawInfo(autofill::ADDRESS_HOME_STATE));
    return grey_text([NSString stringWithFormat:@"%@\n%@", name, state]);
  } else {
    return grey_text(name);
  }
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

// Slowly type characters using the keyboard by waiting between each tap.
void SlowlyTypeText(NSString* text) {
  for (NSUInteger i = 0; i < [text length]; ++i) {
    // Wait some time before typing the character.
    base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(200));
    // Type a single character so the user input can be effective.
    [ChromeEarlGrey
        simulatePhysicalKeyboardEvent:[text
                                          substringWithRange:NSMakeRange(i, 1)]
                                flags:0];
  }
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
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
}

- (void)tearDownHelper {
  // Close tabs  before clearing the stores in the case the
  // stores are needed when the tabs are closing (e.g. to upload votes).
  [[self class] closeAllTabs];

  GREYAssertTrue([PasswordManagerAppInterface clearCredentials],
                 @"Clearing credentials wasn't done.");
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface clearProfilesStore];

  // Clean up histogram tester.
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(
      autofill::features::test::kAutofillServerCommunication);
  config.features_enabled.push_back(kIOSKeyboardAccessoryUpgradeForIPad);
  if ([self isRunningTest:@selector(testFillXframeCreditCardForm)] ||
      [self isRunningTest:@selector(testFillXframeCreditCardFormThrottled)] ||
      [self isRunningTest:@selector
            (testFillXframeCreditCardForm_WithPaymentSheetFix)]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillAcrossIframesIos);
  }
  if ([self isRunningTest:@selector(testFillXframeCreditCardFormThrottled)]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillAcrossIframesIosThrottling);
  }
  if ([self isRunningTest:@selector
            (testFillCreditCardFieldsOnForm_WithUserEditedFix_UserEdited)] ||
      [self isRunningTest:@selector
            (testFillCreditCardFieldsOnForm_WithUserEditedFix_NotUserEdited)]) {
    config.features_enabled.push_back(
        kAutofillCorrectUserEditedBitInParsedField);
  }
  if ([self isRunningTest:@selector(testAddressHomeAndWorkIPH)]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillEnableSupportForHomeAndWork);
    config.iph_feature_enabled =
        feature_engagement::kIPHAutofillHomeWorkProfileSuggestionFeature.name;
  }

  if ([self isRunningTest:@selector(testReFillAddressFieldsOnForm)]) {
    config.features_enabled.push_back(kAutofillRefillForFormsIos);
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

// Loads a page with a xframe credit card that busts the limit of frames.
- (void)loadThrottledXframePaymentPage {
  // Loads page with xframe credit card from.
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/xframe_credit_card_throttled.html")];
  [ChromeEarlGrey
      waitForWebStateContainingText:
          "Autofill Test - Xframe Credit Card - Frame Limit Exceeded"];

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

// Loads simple address page with refill on localhost.
- (void)loadRefillAddressPage {
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/autofill_refill_test.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Refill Profile Autofill"];
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
// Verifies child frames of the main frame when `frameId` is not empty.
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

  id<GREYMatcher> user_chip =
      KeyboardAccessoryPasswordSuggestion(self.testServer);

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

  id<GREYMatcher> user_chip =
      KeyboardAccessoryPasswordSuggestion(self.testServer);

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

  id<GREYMatcher> user_chip =
      KeyboardAccessoryPasswordSuggestion(self.testServer);

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

  // Wait for the keyboard accessory to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      manual_fill::FormSuggestionViewMatcher()];

  // Scroll to the right of the keyboard accessory so that the second card
  // suggestion is visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];

  id<GREYMatcher> cc_chip = KeyboardAccessoryCreditCardSuggestion();
  [[EarlGrey selectElementWithMatcher:cc_chip] performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self verifyCreditCardInfosHaveBeenFilled:autofill::test::GetCreditCard()];

  // Verify that the acceptance of the card suggestion at index 1 was correctly
  // recorded.
  CheckCardAutofillSuggestionAcceptedIndexMetricsCount(/*suggestion_index=*/1);

  [AutofillAppInterface clearMockReauthenticationModule];
}

// Tests that the fix on the is_user_edited bit in the parsed form fields is
// effective in the case the user has edited the input field for real.
- (void)testFillCreditCardFieldsOnForm_WithUserEditedFix_UserEdited {
  // Fill using another test. The CVC number won't be filled because a local
  // card is used.
  [self testFillCreditCardFieldsOnForm];

  // Focus on the cvc field to fill it.
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:
                      @"document.getElementById('cvc').focus();"];
  // Wait some time so the keyboard has time to show up then slowly type the CVC
  // number.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(200));
  SlowlyTypeText(@"123");

  // Submit so the perfect fill metric is recorded.
  [ChromeEarlGrey tapWebStateElementWithID:@"Submit"];

  // Verify the perfect fill metric in a wait loop to let the time to the submit
  // event to be propagated down to the browser. A not perfect fill should be
  // recorded because the user has manually edited the CVC field which wasn't
  // filled.
  GREYAssertTrue(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::Seconds(2),
          ^() {
            return [MetricsAppInterface
                       expectUniqueSampleWithCount:1
                                         forBucket:0
                                      forHistogram:@"Autofill.PerfectFilling."
                                                   @"CreditCards"] == nil;
          }),
      @"Autofill.PerfectFilling.CreditCards verification failed");
}

// Tests that the fix on the is_user_edited bit in the parsed form fields is
// effective in the case the user didn't edit the fields that weren't filled.
- (void)testFillCreditCardFieldsOnForm_WithUserEditedFix_NotUserEdited {
  // Fill using another test. The CVC number won't be filled because a local
  // card is used.
  [self testFillCreditCardFieldsOnForm];

  // Submit so the perfect fill metric is recorded.
  [ChromeEarlGrey tapWebStateElementWithID:@"Submit"];

  // Verify the perfect fill metric in a wait loop to let the time to the submit
  // event to be propagated down to the browser. A perfect fill should be
  // recorded because the user didn't edit the unfilled CVC field.
  GREYAssertTrue(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::Seconds(2),
          ^() {
            return [MetricsAppInterface
                       expectUniqueSampleWithCount:1
                                         forBucket:1
                                      forHistogram:@"Autofill.PerfectFilling."
                                                   @"CreditCards"] == nil;
          }),
      @"Autofill.PerfectFilling.CreditCards verification failed");
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

  // Tap on the credit card chip.
  id<GREYMatcher> cc_chip = KeyboardAccessoryCreditCardSuggestion();
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

  autofill::CreditCard card = autofill::test::GetCreditCard();
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

// Tests that child frame throttling can be enforced for xframe credit card
// form.
- (void)testFillXframeCreditCardFormThrottled {
  // Mock reauth so it allows filling sensitive information without the need for
  // real authentication.
  [AutofillAppInterface setUpMockReauthenticationModule];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];

  // Load the xframe payment page.
  [self loadThrottledXframePaymentPage];

  // Tap a credit card field to open the KA. The bottom sheet isn't triggered
  // where there is only a name field in the credit card form.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];

  // Tap on the credit card chip.
  id<GREYMatcher> cc_chip = KeyboardAccessoryCreditCardSuggestion();
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:cc_chip];
  [[EarlGrey selectElementWithMatcher:cc_chip] performAction:grey_tap()];

  // Verify that the credit card fields were filled correctly across frames,
  // where the name field should be filled but the other fields should remain
  // unfilled because the limit of child frames was busted which results in not
  // building the frame tree for the form.

  std::string locale = l10n_util::GetLocaleOverride();

  // Verify that the cardholder name field on the main frame was filled which
  // doesn't require the frame tree.
  autofill::CreditCard card = autofill::test::GetCreditCard();
  [self verifyFieldWithIdHasBeenFilled:kFormCardName
                              iframeId:""
                                 value:base::SysUTF16ToNSString(card.GetInfo(
                                           autofill::CREDIT_CARD_NAME_FULL,
                                           locale))];

  // Verify that the fields on other frames weren't filled as they require the
  // frame tree which wasn't constructed because the limit of frames was busted.
  std::vector<std::tuple<autofill::FieldType, std::string, std::string>>
      empty_fields_to_verify = {
          std::make_tuple(autofill::CREDIT_CARD_NUMBER, "cc-number-frame",
                          kFormCardNumber),
          std::make_tuple(autofill::CREDIT_CARD_EXP_MONTH, "cc-exp-frame",
                          kFormCardExpirationMonth),
          std::make_tuple(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
                          "cc-exp-frame", kFormCardExpirationYear),
  };
  for (const auto& [field_type, frame_id_attr, field_id_attr] :
       empty_fields_to_verify) {
    [self verifyFieldWithIdHasBeenFilled:field_id_attr
                                iframeId:frame_id_attr
                                   value:@""];
  }

  // Cleanup.
  [AutofillAppInterface clearMockReauthenticationModule];
}

// Tests that the bottom sheet cohabitates well with other non-credit card forms
// when the fix for the payment sheet across iframes is enabled. This makes sure
// that crbug.com/417449733 doesn't occur.
- (void)testFillXframeCreditCardForm_WithPaymentSheetFix {
  // Mock reauth so it allows filling sensitive information without the need for
  // real authentication.
  [AutofillAppInterface setUpMockReauthenticationModule];
  [AutofillAppInterface mockReauthenticationModuleCanAttempt:YES];
  [AutofillAppInterface mockReauthenticationModuleExpectedResult:
                            ReauthenticationResult::kSuccess];

  // Load the xframe payment page.
  [self loadXframePaymentPage];

  // Give time for the page to settle even if at this point we could read
  // content in the DOM. This is to prevent flakes.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));

  // Tap on the first address field and verify that the keyboard pops up and
  // offers address suggestions. name_address
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("name_address")];
  id<GREYMatcher> name_chip =
      KeyboardAccessoryAddressSuggestion(autofill::NAME_FULL);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:name_chip];

  // Tap on another address field, below the first address field, and verify
  // that the keyboard pops up and offers address suggestions.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("city")];
  id<GREYMatcher> address_chip =
      KeyboardAccessoryAddressSuggestion(autofill::ADDRESS_HOME_CITY);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:address_chip];

  // Verify that the payment bottom sheet is still displayed when tapping on
  // credit card fields.

  // Tap a credit card field to open the bottom sheet.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormCardName)];
  id<GREYMatcher> useKeyboardButton = PaymentsBottomSheetUseKeyboardButton();
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:useKeyboardButton];

  // Dismiss the bottom sheet and open the keyboard.
  [[EarlGrey selectElementWithMatcher:useKeyboardButton]
      performAction:grey_tap()];

  // Verify that filling the payment form still works correctly.

  // Tap on the credit card chip.
  id<GREYMatcher> cc_chip = KeyboardAccessoryCreditCardSuggestion();
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

  autofill::CreditCard card = autofill::test::GetCreditCard();
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

  id<GREYMatcher> address_chip =
      KeyboardAccessoryAddressSuggestion(autofill::ADDRESS_HOME_ZIP);

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:address_chip];

  [[EarlGrey selectElementWithMatcher:address_chip] performAction:grey_tap()];

  // Verify that the page is filled properly.
  [self verifyAddressInfosHaveBeenFilled:autofill::test::GetFullProfile()];

  // Verify that the acceptance of the address suggestion at index 0 was
  // correctly recorded.
  CheckAddressAutofillSuggestionAcceptedIndexMetricsCount(
      /*suggestion_index=*/0);
}

// Tests that tapping on a name field of a dinamically expanding address form
// and accepting the keyboard accessory suggestion automatically autofills the
// whole address.
- (void)testReFillAddressFieldsOnForm {
  [self loadRefillAddressPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormName)];

  id<GREYMatcher> name_chip = KeyboardAccessoryNameSuggestion();

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:name_chip];

  // Autofill the name field to uncover the rest of the address form.
  [[EarlGrey selectElementWithMatcher:name_chip] performAction:grey_tap()];

  // Verify that the whole address was filled properly.
  [self verifyAddressInfosHaveBeenFilled:autofill::test::GetFullProfile()];
}

// Tests the IPH feature for a Home and Work account profile.
- (void)testAddressHomeAndWorkIPH {
  // Delete the profile that is added on `-setUp`.
  [AutofillAppInterface clearProfilesStore];
  // Store one address.
  [AutofillAppInterface saveExampleHomeWorkAccountProfile];

  [self loadAddressPage];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormZip)];

  id<GREYMatcher> iph_chip = grey_text(l10n_util::GetNSString(
      IDS_AUTOFILL_IPH_HOME_AND_WORK_ACCOUNT_PROFILE_SUGGESTION));

  // Ensure the Home and Work suggestion IPH appears.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:iph_chip];
}

// Tests that the manual fill button opens the expanded manual fill view.
- (void)testOpenExpandedManualFillView {
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

// Tests that the manual fill button title is hidden in compact mode (tablets
// only).
- (void)testManualFillButtonTitleIsHiddenInCompactMode {
  if (![ChromeEarlGrey areMultipleWindowsSupported] ||
      ![AutofillAppInterface isKeyboardAccessoryUpgradeEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPhone (the manual fill button has no title on iPhone) "
        @"or when the Keyboard Accessory Upgrade feature is disabled.");
  }

  [self loadAddressPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormZip)];

  id<GREYMatcher> manual_fill_button = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_AUTOFILL_DATA));
  id<GREYMatcher> manual_fill_button_title = grey_text(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_ALL_AUTOFILL_DATA));

  // Verify that the manual fill button is visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:manual_fill_button];

  // Verify that the manual fill button title is visible.
  [[EarlGrey selectElementWithMatcher:manual_fill_button_title]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Make the window compact by using split screen.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Verify that the manual fill button is still visible.
  [[EarlGrey selectElementWithMatcher:manual_fill_button]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the manual fill button has no title.
  [[EarlGrey selectElementWithMatcher:manual_fill_button_title]
      assertWithMatcher:grey_notVisible()];

  // Exit split screen.
  [ChromeEarlGrey closeWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:1];
}

@end

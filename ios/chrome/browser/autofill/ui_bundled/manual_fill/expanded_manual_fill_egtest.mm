// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string_view>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/plus_addresses/core/common/features.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_matchers.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/credential_suggestion_bottom_sheet_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_ui_features.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings_app_interface.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ActionSheetItemWithAccessibilityLabelId;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using manual_fill::ChipButton;
using manual_fill::ExpandedManualFillHeaderView;
using manual_fill::ExpandedManualFillView;
using manual_fill::KeyboardAccessoryManualFillButton;
using manual_fill::ManualFillDataType;
using manual_fill::SegmentedControlAddressTab;
using manual_fill::SegmentedControlPasswordTab;
using net::test_server::EmbeddedTestServer;

namespace {
constexpr char kAddressFormURL[] = "/profile_form.html";
constexpr char kMultiFieldFormURL[] = "/multi_field_form.html";
constexpr char kMultiFormPageURL[] = "/multi_form_page.html";
constexpr char kPaymentMethodFormURL[] = "/credit_card.html";
constexpr char kPasswordFormURL[] = "/simple_login_form.html";

const char kCardNameFieldID[] = "CCName";
const char kNameFieldID[] = "name";
const char kOtherStuffFieldID[] = "otherstuff";
const char kPasswordFieldID[] = "pw";

// Matcher for the "Autofill Form" button shown in the cells.
id<GREYMatcher> AutofillFormButton() {
  return grey_allOf(grey_accessibilityID(
                        manual_fill::kExpandedManualFillAutofillFormButtonID),
                    grey_interactable(), nullptr);
}

// Matcher for the close button.
id<GREYMatcher> CloseButton() {
  return grey_accessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_CLOSE_BUTTON_ACCESSIBILITY_LABEL));
}

id<GREYMatcher> ExpandedManualFillHeaderView() {
  return grey_accessibilityID(manual_fill::kExpandedManualFillHeaderViewID);
}

// Matcher for the segmented control's payment method tab.
id<GREYMatcher> SegmentedControlPaymentMethodTab() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_EXPANDED_MANUAL_FILL_PAYMENT_TAB_ACCESSIBILITY_LABEL)),
      grey_ancestor(ExpandedManualFillHeaderView()), nil);
}

// Matcher for the keyboard accessory's password icon.
id<GREYMatcher> KeyboardAccessoryPasswordManualFillButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_AUTOFILL_PASSWORD_AUTOFILL_DATA)),
                    grey_ancestor(grey_accessibilityID(
                        kFormInputAccessoryViewAccessibilityID)),
                    nil);
}

// Matcher for the password suggestion chip.
id<GREYMatcher> KeyboardAccessoryPasswordSuggestionChip(
    EmbeddedTestServer* test_server) {
  NSString* username = @"concrete username";
  if ([ChromeEarlGrey isIPadIdiom]) {
    // On iPad, the suggestion text is an attributed string containing the
    // signon realm on the 2nd line.
    NSString* realm = base::SysUTF8ToNSString(password_manager::GetShownOrigin(
        url::Origin::Create(test_server->base_url())));
    return grey_text([NSString stringWithFormat:@"%@\n%@", username, realm]);
  } else {
    return grey_text(username);
  }
}

// Checks that the chip button with `title` is sufficiently visible.
void CheckChipButtonVisibility(std::u16string title) {
  [[EarlGrey selectElementWithMatcher:ChipButton(title)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that the header view is as expected according to whether or not the
// device is in landscape mode.
void CheckHeader(bool is_landscape) {
  [[EarlGrey selectElementWithMatcher:ExpandedManualFillHeaderView()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // The header's top view should only be part of the UI when in portrait mode.
  id<GREYMatcher> header_top_view =
      grey_accessibilityID(manual_fill::kExpandedManualFillHeaderTopViewID);
  // On iPad, the top view is always visible.
  bool is_top_view_visible = [ChromeEarlGrey isIPadIdiom] || !is_landscape;
  [[EarlGrey selectElementWithMatcher:header_top_view]
      assertWithMatcher:is_top_view_visible ? grey_sufficientlyVisible()
                                            : grey_notVisible()];

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
  [[EarlGrey selectElementWithMatcher:SegmentedControlPasswordTab()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SegmentedControlPaymentMethodTab()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SegmentedControlAddressTab()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Loads a form depending on the desired `data_type`.
void LoadForm(EmbeddedTestServer* test_server, ManualFillDataType data_type) {
  std::string_view form_url;
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
    case ManualFillDataType::kOther:
      form_url = kMultiFieldFormURL;
      form_text = "hello!";
      break;
  }

  [ChromeEarlGrey loadURL:test_server->GetURL(form_url)];
  [ChromeEarlGrey waitForWebStateContainingText:form_text];
}

// Loads a page with forms for different data types.
void LoadMultiFormPage(EmbeddedTestServer* test_server) {
  [ChromeEarlGrey loadURL:test_server->GetURL(kMultiFormPageURL)];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];
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
void MakeSurePaymentMethodSuggestionsAreVisible() {
  // Needed in order to see the payment method suggestions.
  [AutofillAppInterface considerCreditCardFormSecureForTesting];

  DismissPaymentBottomSheet();

  // Wait for the payment suggestions to appear after dismissing the payment
  // bottom sheet.
  autofill::CreditCard card = autofill::test::GetCreditCard();
  NSString* cc_text = base::SysUTF16ToNSString(card.GetInfo(
      autofill::CREDIT_CARD_NAME_FULL, l10n_util::GetLocaleOverride()));
  if ([ChromeEarlGrey isIPadIdiom]) {
    // On iPad, the suggestion text is an attributed string containing the
    // obfuscated credit card on the 2nd line.
    NSString* cc_network = base::SysUTF16ToNSString(
        card.NetworkAndLastFourDigits(/*obfuscation_length=*/2));
    cc_text = [NSString stringWithFormat:@"%@\n%@", cc_text, cc_network];
  }
  id<GREYMatcher> cc_chip = grey_text(cc_text);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:cc_chip];
}

// Looks for the "Autofill form" button in the provided `scroll_view`.
GREYElementInteraction* SearchAutofillFormButton(id<GREYMatcher> scroll_view) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(AutofillFormButton(),
                                          grey_sufficientlyVisible(), nullptr)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:scroll_view];
}

}  // namespace

// Test case for the expanded manual fill view.
@interface ExpandedManualFillTestCase : WebHttpServerChromeTestCase
@end

@implementation ExpandedManualFillTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_disabled.push_back(
      plus_addresses::features::kPlusAddressesEnabled);

  return config;
}

- (void)setUp {
  [super setUp];

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  [AutofillAppInterface clearCreditCardStore];

  // Save a password, credit card and address.
  SavePasswordForLoginForm(self.testServer);
  [AutofillAppInterface saveLocalCreditCard];
  [AutofillAppInterface saveExampleAccountProfile];

  // Disable the credential bottom sheet.
  [CredentialSuggestionBottomSheetAppInterface disableBottomSheet];

  // Mock successful reauth for opening the Password Manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
}

- (void)tearDownHelper {
  [super tearDownHelper];

  // Clear the stores.
  [AutofillAppInterface clearProfilePasswordStore];
  [AutofillAppInterface clearCreditCardStore];
  [AutofillAppInterface clearProfilesStore];

  [PasswordSettingsAppInterface removeMockReauthenticationModule];
}

// Loads the appropriate form for the passed `dataType` and opens the expanded
// manual fill view from there.
- (void)loadFormAndOpenExpandedManualFillViewForDataType:
            (ManualFillDataType)dataType
                                             fieldToFill:
                                                 (std::string)fieldToFill {
  LoadForm(self.testServer, dataType);
  [AutofillAppInterface considerCreditCardFormSecureForTesting];
  [self openExpandedManualFillViewForDataType:dataType fieldToFill:fieldToFill];
}

// Opens the expanded manual fill view for a given `dataType`. `fieldToFill` is
// the ID of the form field that should be focused prior to opening the expanded
// manual fill view.
- (void)openExpandedManualFillViewForDataType:(ManualFillDataType)dataType
                                  fieldToFill:(std::string)fieldToFill {
  // Tap on the provided field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(fieldToFill)];

  if (dataType == ManualFillDataType::kPaymentMethod) {
    MakeSurePaymentMethodSuggestionsAreVisible();
  }

  // Open the expanded manual fill view.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:KeyboardAccessoryManualFillButton()];
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryManualFillButton()]
      performAction:grey_tap()];

  // Confirm that the expanded manual fill view is visible.
  [[EarlGrey selectElementWithMatcher:ExpandedManualFillView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)openAllPasswordListFromPasswordTab {
  // Tap the "Select Password..." action.
  [[EarlGrey selectElementWithMatcher:manual_fill::OtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Acknowledge concerns using other passwords on a website.
  id<GREYMatcher> confirmDialogButton =
      grey_allOf(ActionSheetItemWithAccessibilityLabelId(
                     IDS_IOS_CONFIRM_USING_OTHER_PASSWORD_CONTINUE),
                 grey_interactable(), nullptr);
  [[EarlGrey selectElementWithMatcher:confirmDialogButton]
      performAction:grey_tap()];

  // Verify that the all password list is visible.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::OtherPasswordsDismissMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Tests

// Tests that the expanded manual fill view header is correctly laid out
// according to the device's orientation.
- (void)testExpandedManualFillViewDeviceOrientation {
  [self loadFormAndOpenExpandedManualFillViewForDataType:ManualFillDataType::
                                                             kPassword
                                             fieldToFill:kPasswordFieldID];

  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationLandscapeRight
                                   error:nil];
  CheckHeader(/*is_landscape=*/true);

  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationLandscapeLeft
                                   error:nil];
  CheckHeader(/*is_landscape=*/true);

  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationPortrait
                                   error:nil];
  CheckHeader(/*is_landscape=*/false);
}

// Tests that password manual filling options are visible when the expanded
// manual fill view is opened from a field for which there are password
// suggestions.
- (void)testOpeningExpandedManualFillViewForPassword {
  // Open the expanded manual fill view for a password field.
  [self loadFormAndOpenExpandedManualFillViewForDataType:ManualFillDataType::
                                                             kPassword
                                             fieldToFill:kPasswordFieldID];

  // The password view controller should be visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that payment method manual filling options are visible when the
// expanded manual fill view is opened from a field for which there are payment
// method suggestions.
- (void)testOpeningExpandedManualFillViewForPaymentMethod {
  // Open the expanded manual fill view for a payment method field.
  [self loadFormAndOpenExpandedManualFillViewForDataType:ManualFillDataType::
                                                             kPaymentMethod
                                             fieldToFill:kCardNameFieldID];

  // The payment method view controller should be visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that address manual filling options are visible when the expanded
// manual fill view is opened from a field for which there are address
// suggestions.
- (void)testOpeningExpandedManualFillViewForAddress {
  // Open the expanded manual fill view for an address field.
  [self loadFormAndOpenExpandedManualFillViewForDataType:ManualFillDataType::
                                                             kAddress
                                             fieldToFill:kNameFieldID];

  // The address view controller should be visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::ProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the right manual filling options are visible when switching from
// one data type to the other.
- (void)testSwitchingDataTypes {
  [self loadFormAndOpenExpandedManualFillViewForDataType:ManualFillDataType::
                                                             kPassword
                                             fieldToFill:kPasswordFieldID];

  // Select the address tab and confirm that the address view controller is
  // visible.
  [[EarlGrey selectElementWithMatcher:SegmentedControlAddressTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:manual_fill::ProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select the payment method tab and confirm that the payment method view
  // controller is visible.
  [[EarlGrey selectElementWithMatcher:SegmentedControlPaymentMethodTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:manual_fill::CreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select the password tab and confirm that the password view controller is
  // visible.
  [[EarlGrey selectElementWithMatcher:SegmentedControlPasswordTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping the close button hides the expanded manual fill view to
// show the keyboard and keyboard accessory bar.
- (void)testClosingExpandedManualFillView {
  [self loadFormAndOpenExpandedManualFillViewForDataType:ManualFillDataType::
                                                             kPassword
                                             fieldToFill:kPasswordFieldID];

  // Tap the close button.
  [[EarlGrey selectElementWithMatcher:CloseButton()] performAction:grey_tap()];

  // The expanded manual fill view should not be visible anymore.
  [[EarlGrey selectElementWithMatcher:ExpandedManualFillView()]
      assertWithMatcher:grey_notVisible()];

  // The keyboard accessory and keyboard should be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      KeyboardAccessoryPasswordSuggestionChip(self.testServer)];
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryManualFillButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [ChromeEarlGrey waitForKeyboardToAppear];
}

// Tests that saved passwords for the current site are visible even when the
// expanded manual fill view was not initially opened from a password form.
- (void)testPasswordsVisibleWhenOpenedFromDifferentDataType {
  // Open the expanded manual fill view for an address field.
  [self loadFormAndOpenExpandedManualFillViewForDataType:ManualFillDataType::
                                                             kAddress
                                             fieldToFill:kNameFieldID];

  // Select the password tab and confirm that the password view controller is
  // visible.
  [[EarlGrey selectElementWithMatcher:SegmentedControlPasswordTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:manual_fill::PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Confirm that the password option is visible.
  CheckChipButtonVisibility(u"concrete username");
}

// Tests that the "Autofill Form" button does not exist for the other data types
// than payments if a payments field is in focus.
- (void)testNoAutofillFormButtonForNonPaymentTypes {
  // Open the expanded manual fill view for a payment field.
  [self loadFormAndOpenExpandedManualFillViewForDataType:ManualFillDataType::
                                                             kPaymentMethod
                                             fieldToFill:kCardNameFieldID];

  // Check that the "Autofill Form" button exists.
  [SearchAutofillFormButton(manual_fill::CreditCardTableViewMatcher())
      assertWithMatcher:grey_sufficientlyVisible()];

  // Navigate to the address tab and check that the "Autofill Form" button does
  // not exist.
  [[EarlGrey selectElementWithMatcher:SegmentedControlAddressTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_notVisible()];

  // Navigate to the password tab and check that the "Autofill Form" button does
  // not exist.
  [[EarlGrey selectElementWithMatcher:SegmentedControlPasswordTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_notVisible()];

  // Navigate to the all password list and check that the "Autofill Form"
  // button does not exist.
  [self openAllPasswordListFromPasswordTab];
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the "Autofill Form" button does not exist for the other data types
// than addresses if an address field is in focus.
- (void)testNoAutofillFormButtonForNonAddressTypes {
  // Open the expanded manual fill view for an address field.
  [self loadFormAndOpenExpandedManualFillViewForDataType:ManualFillDataType::
                                                             kAddress
                                             fieldToFill:kNameFieldID];

  // Check that the "Autofill Form" button exists.
  [SearchAutofillFormButton(manual_fill::ProfilesTableViewMatcher())
      assertWithMatcher:grey_sufficientlyVisible()];

  // Navigate to the payment tab and check that the "Autofill Form" button does
  // not exist.
  [[EarlGrey selectElementWithMatcher:SegmentedControlPaymentMethodTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_notVisible()];

  // Navigate to the password tab and check that the "Autofill Form" button does
  // not exist.
  [[EarlGrey selectElementWithMatcher:SegmentedControlPasswordTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_notVisible()];

  // Navigate to the all password list and check that the "Autofill Form"
  // button does not exist.
  [self openAllPasswordListFromPasswordTab];
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the "Autofill Form" button does not exist for the other data types
// than passwords if a password field is in focus.
- (void)testNoAutofillFormButtonForNonPasswordTypes {
  // Open the expanded manual fill view for a password field.
  [self loadFormAndOpenExpandedManualFillViewForDataType:ManualFillDataType::
                                                             kPassword
                                             fieldToFill:kPasswordFieldID];

  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Navigate to the address tab and check that the "Autofill Form" button does
  // not exist.
  [[EarlGrey selectElementWithMatcher:SegmentedControlAddressTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_notVisible()];

  // Navigate to the payment tab and check that the "Autofill Form" button does
  // not exist.
  [[EarlGrey selectElementWithMatcher:SegmentedControlPaymentMethodTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_notVisible()];

  // Go back to the password tab to open the all password list. Check that the
  // "Autofill Form" button exists.
  [[EarlGrey selectElementWithMatcher:SegmentedControlPasswordTab()]
      performAction:grey_tap()];
  [self openAllPasswordListFromPasswordTab];
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Autofill Form" button does not exist for all of the data
// types if the type of the focused field can't be associated with any of them.
- (void)testNoAutofillFormButtonForRandomType {
  // Load form.
  LoadForm(self.testServer, ManualFillDataType::kOther);

  // Tap on a field that's not associated to password, payment or address.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kOtherStuffFieldID)];

  // Open the expanded manual fill view by tapping the password icon.
  [[EarlGrey
      selectElementWithMatcher:KeyboardAccessoryPasswordManualFillButton()]
      performAction:grey_tap()];

  // Check that the "Autofill Form" button does not exist.
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_notVisible()];

  // Move to the address tab and check that the "Autofill Form" button does not
  // exist.
  [[EarlGrey selectElementWithMatcher:SegmentedControlAddressTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_notVisible()];

  // Move to the payment tab and check that the "Autofill Form" button does not
  // exist.
  [[EarlGrey selectElementWithMatcher:SegmentedControlPaymentMethodTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the "Autofill Form" button's visibility correctly updates as
// the focused field on the webpage changes. The button should only be
// visible in the expanded manual fill view when the selected data type tab
// matches the focused field's type.
- (void)testAutofillFormButtonVisibilityChangesWithFocusedField {
  // Not applicable for iPad as interacting with anything outside of the
  // expanded manual fill view makes the view disappear.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test not applicable for iPad.");
  }

  // Load the multi form page and open the expanded manual fill view for an
  // address field.
  LoadMultiFormPage(self.testServer);
  [self openExpandedManualFillViewForDataType:ManualFillDataType::kAddress
                                  fieldToFill:kNameFieldID];

  // Check that the "Autofill Form" button exists.
  [SearchAutofillFormButton(manual_fill::ProfilesTableViewMatcher())
      assertWithMatcher:grey_sufficientlyVisible()];

  // Now focus a password-related field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kPasswordFieldID)];

  // The "Autofill form" button should have disappeared.
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_nil()];

  // Navigate to the password tab and check that the "Autofill Form" button does
  // exist as the focused field is password-related.
  [[EarlGrey selectElementWithMatcher:SegmentedControlPasswordTab()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AutofillFormButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end

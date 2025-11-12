// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <string_view>

#import "base/ios/ios_util.h"
#import "base/strings/strcat.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/autofill_debug_features.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/constants.h"
#import "components/autofill/ios/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/bottom_sheet_constants.h"
#import "ios/chrome/browser/badges/model/features.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/infobar_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_address_profile_modal_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

// URLs of the test pages.
constexpr char kProfileForm[] = "/autofill_smoke_test.html";
constexpr char kXframeFormPage[] = "/xhr_xframe_submit.html";
constexpr char kFullAddressFormPage[] = "/full_address_form.html";

// Ids of fields in the form.
constexpr char kFormElementName[] = "form_name";
constexpr char kFormElementEmail[] = "form_email";
constexpr char kFormElementSubmit[] = "submit_profile";

// Minimal cooldown period to wait for between typing characters. The bare
// minimum should be the frame rate, something aroung 17ms (1/60hz), but we
// prefer giving extra buffer as there are probably other latencies.
constexpr base::TimeDelta kTypingCoolDownPeriod = base::Milliseconds(50);

// Email value used by the tests.
constexpr std::string_view kEmail = "foo1@gmail.com";

// Histogram bucket representing renderer errors.
constexpr int kRendererErrorHistogramBucket = 8;

struct FullAddressFormPageParams {
  // True if the submission should be default prevented.
  bool default_prevented = false;
  // True if there should be redirection done after submitting with
  // a parameter that can stop the submit event from being handled by Autofill.
  bool redirect = false;
  // True if the submission should be prevented from propagating to any other
  // listener regardless of their positioning.
  bool stop_immediate_propagation = false;
  // True if multiple submissions should be scheduled.
  bool multiple_submissions = false;
  // True if when using `multiple_submissions` the last programmatic submission
  // should be skipped.
  bool multiple_submissions_skip_programmatic = false;
};

// Matcher for the banner button.
id<GREYMatcher> BannerButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_SAVE_ADDRESS_MESSAGE_PRIMARY_ACTION));
}

// Matcher for the update banner button.
id<GREYMatcher> UpdateBannerButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabel(l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_UPDATE_ADDRESS_MESSAGE_PRIMARY_ACTION));
}

// Matcher for the "Save Address" modal button.
id<GREYMatcher> ModalButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL));
}

// Matcher for the "Update Address" modal button.
id<GREYMatcher> UpdateModalButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabel(l10n_util::GetNSString(
      IDS_AUTOFILL_UPDATE_ADDRESS_ADD_NEW_INFO_PROMPT_OK_BUTTON_LABEL));
}

// Matcher for the modal button.
id<GREYMatcher> ModalEditButtonMatcher() {
  return grey_allOf(grey_accessibilityID(kInfobarSaveAddressModalEditButton),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the migration button in modal view.
id<GREYMatcher> ModalMigrationButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabel(l10n_util::GetNSString(
      IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_OK_BUTTON_LABEL));
}

// Matcher for a country entry with the given accessibility label.
id<GREYMatcher> CountryEntry(NSString* label) {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(label),
                    grey_userInteractionEnabled(), grey_sufficientlyVisible(),
                    nil);
}

// Matcher for the search bar.
id<GREYMatcher> SearchBar() {
  // Match using the accessibility trait for a search field.
  return grey_allOf(grey_accessibilityTrait(UIAccessibilityTraitSearchField),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the search bar's scrim.
id<GREYMatcher> SearchBarScrim() {
  return grey_accessibilityID(kAutofillCountrySelectionSearchScrimId);
}

id<GREYMatcher> TextFieldWithLabel(NSString* textFieldLabel) {
  return grey_allOf(grey_accessibilityID(
                        [textFieldLabel stringByAppendingString:@"_textField"]),
                    grey_kindOfClass([UITextField class]), nil);
}

id<GREYMatcher> EditProfileBottomSheet() {
  return grey_accessibilityID(kEditProfileBottomSheetViewIdentfier);
}

// Slowly type characters using the keyboard by waiting between each tap.
void SlowlyTypeText(NSString* text) {
  for (NSUInteger i = 0; i < [text length]; ++i) {
    // Wait some time before typing the character.
    base::test::ios::SpinRunLoopWithMinDelay(kTypingCoolDownPeriod);
    // Type a single character so the user input can be effective.
    [ChromeEarlGrey
        simulatePhysicalKeyboardEvent:[text
                                          substringWithRange:NSMakeRange(i, 1)]
                                flags:0];
  }
  // Give some cooldown period so the character has the time to be typed
  // before doing something else on the page.
  base::test::ios::SpinRunLoopWithMinDelay(kTypingCoolDownPeriod);
}

void TypeTextInXframeField(NSString* fieldID, NSString* text) {
  // Focus on the field that corresponds to `fieldID` to pop up the keyboard.
  NSString* script = [NSString
      stringWithFormat:@"document.querySelector('iframe')"
                        ".contentDocument.getElementById('%@').focus()",
                       fieldID];
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];

  // Type the `text` on the field.
  SlowlyTypeText(text);
}

}  // namespace

@interface SaveProfileEGTest : ChromeTestCase

@end

@implementation SaveProfileEGTest

- (void)setUp {
  [super setUp];
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
}

- (void)tearDownHelper {
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);

  // Clear existing profile.
  [AutofillAppInterface clearProfilesStore];

  [super tearDownHelper];
}

// TODO(crbug.com/391826905): Re-enable this test on simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testEditBottomSheetAlertBySwipingDown \
  FLAKY_testEditBottomSheetAlertBySwipingDown
#else
#define MAYBE_testEditBottomSheetAlertBySwipingDown \
  testEditBottomSheetAlertBySwipingDown
#endif
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self isRunningTest:@selector(testUserData_AccountSave)] ||
      [self
          isRunningTest:@selector(testUserData_LocalHideBottomSheetOnCancel)]) {
    // These test cases need a badge.
    config.features_disabled.push_back(kAutofillBadgeRemoval);
  }

  config.features_disabled.push_back(
      autofill::features::debug::kAutofillServerCommunication);

  if ([self isRunningTest:@selector(testStickySavePromptJourney)]) {
    config.features_enabled.push_back(kAutofillStickyInfobarIos);
  }

  if ([self isRunningTest:@selector
            (testUserData_AccountSave_AutofillAcrossIframe_XHR)]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillAcrossIframesIos);
    config.features_enabled.push_back(kAutofillFixXhrForXframe);
  }

  if ([self isRunningTest:@selector(testSubmissionErrorReporting_Enabled)]) {
    config.features_enabled.push_back(kAutofillReportFormSubmissionErrors);
  }

  if ([self isRunningTest:@selector(testSubmissionErrorReporting_Disabled)]) {
    config.features_disabled.push_back(kAutofillReportFormSubmissionErrors);
  }

  if ([self isRunningTest:@selector(testUserData_LocalUpdate)]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillEnableSupportForHomeAndWork);
  }

  // TODO(crbug.com/428189566): Re-enable after the test is fixed for
  // ios-fieldtrial-rel.
  if ([self isRunningTest:@selector
            (DISABLED_testSubmissionCountReporting_ScheduledTask)] ||
      [self isRunningTest:@selector
            (DISABLED_testSubmissionCountReporting_UnloadPage)]) {
    config.features_enabled.push_back(kAutofillCountFormSubmissionInRenderer);
  }

  return config;
}

#pragma mark - Test helper methods

// Fills the president profile in the form by clicking on the button, submits
// the form to the save address profile infobar.
- (void)fillPresidentProfileAndShowSaveInfobar {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kProfileForm)];

  // Ensure there are no saved profiles.
  GREYAssertEqual(0U, [AutofillAppInterface profilesCount],
                  @"There should be no saved profile.");

  [ChromeEarlGrey tapWebStateElementWithID:@"fill_profile_president"];
  [ChromeEarlGrey tapWebStateElementWithID:@"submit_profile"];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];
}

// Triggers the save infobar via XHR submission.
- (void)triggerSaveInfobarViaXHRSubmission {
  // Load the xframe address form page.
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kXframeFormPage)];

  // Ensure there are no saved profiles at this point - make sure we start from
  // a clean slate.
  GREYAssertEqual(0U, [AutofillAppInterface profilesCount],
                  @"There should be no saved profile.");

  // Manually type the text in the fields so profile saving can be triggered
  // when autofill across iframes is enabled which require manually editing the
  // fields.
  TypeTextInXframeField(@"form_name", @"User");
  TypeTextInXframeField(@"form_address", @"1234 Pkw Ave");
  TypeTextInXframeField(@"form_city", @"MuteCity");
  TypeTextInXframeField(@"form_zip", @"12345");

  // Trigger XHR submission in the child frame using the dedicated button in the
  // main frame.
  [ChromeEarlGrey tapWebStateElementWithID:@"do-xhr-submit"];

  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];
}

- (void)fillPresidentProfileAndShowSaveModal {
  [self fillPresidentProfileAndShowSaveInfobar];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];
}

// Fills, submits the form and saves the address profile.
- (void)fillFormAndSaveProfile {
  [self fillPresidentProfileAndShowSaveModal];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");
}

// Loads, fills, and submits the full address form.
- (void)loadFullAddressFormWithParams:(FullAddressFormPageParams)params {
  // Start server.
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  auto makeQueryString = [](FullAddressFormPageParams params) -> std::string {
    std::vector<std::string_view> queryParameters;
    if (params.default_prevented) {
      queryParameters.push_back("preventDefault");
    }
    if (params.redirect) {
      queryParameters.push_back("redirectWhenSubmissionPrevented");
    }
    if (params.stop_immediate_propagation) {
      queryParameters.push_back("stopImmediatePropagation");
    }
    if (params.multiple_submissions) {
      queryParameters.push_back("triggerMultipleSubmissions");
    }
    if (params.multiple_submissions_skip_programmatic) {
      queryParameters.push_back("triggerMultipleSubmissionsNoProgrammatic");
    }
    return base::JoinString(queryParameters, "&");
  };

  // Get the URL for the served test page with the query parameters for setting
  // it up.
  const GURL baseURL = self.testServer->GetURL(kFullAddressFormPage);
  GURL::Replacements replacements;
  std::string query = makeQueryString(params).c_str();
  replacements.SetQueryStr(query);
  const GURL fullURL = baseURL.ReplaceComponents(replacements);

  // Load the URL and wait for its content to be loaded.
  [ChromeEarlGrey loadURL:fullURL];

  // Wait until the expected content is loaded in the DOM. If the page is in an
  // error state this verification will fail.
  NSString* wait_content_script =
      @"document.body.innerText.includes('Address Form Test Page')";
  [ChromeEarlGrey waitForJavaScriptCondition:wait_content_script];

  // Call the helper function embedded in the page content to fill the form.
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:@"FillForm();"];
}

- (void)loadAndSubmitFullAddressFormWithParams:
    (FullAddressFormPageParams)params {
  [self loadFullAddressFormWithParams:params];
  // Submit the form via the dedicated <button>.
  [ChromeEarlGrey tapWebStateElementWithID:@"submit-button"];
}

// Focuses on the name field and initiates autofill on the form with the saved
// profile.
- (void)focusOnNameAndAutofill {
  // Ensure there is a saved local profile.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"There should a saved local profile.");

  // Tap on a field to trigger form activity.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Wait for the keyboard to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Wait for suggestions as it may take some time because of form fetch
  // throttling or other delays.
  [ChromeEarlGrey
      waitForMatcher:chrome_test_util::AutofillSuggestionViewMatcher()];

  // Tap on the suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          AutofillSuggestionViewMatcher()]
      performAction:grey_tap()];

  // Verify Web Content was filled.
  NSString* javaScriptCondition = [NSString
      stringWithFormat:@"document.getElementById('%s').value.length > 0",
                       kFormElementName];
  [ChromeEarlGrey waitForJavaScriptCondition:javaScriptCondition];
}

#pragma mark - Tests

// Ensures that the profile is updated after submitting the form.
- (void)testUserData_LocalUpdate {
  // Save a profile.
  [self fillFormAndSaveProfile];

  // Load the form.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kProfileForm)];

  // Autofill the form.
  [self focusOnNameAndAutofill];

  // Tap on email field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementEmail)];

  // Wait for the keyboard to appear.
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Populate the email field.
  // TODO(crbug.com/40916974): This should use grey_typeText when fixed.
  for (char letter : kEmail) {
    if (letter == '@') {
      [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"@"
                                              flags:UIKeyModifierShift];
      continue;
    }

    [ChromeEarlGrey
        simulatePhysicalKeyboardEvent:[NSString stringWithFormat:@"%c", letter]
                                flags:0];
  }

  // Submit the form.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementSubmit)];

  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:UpdateBannerButtonMatcher()]
      performAction:grey_tap()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  // Update the profile.
  [[EarlGrey selectElementWithMatcher:UpdateModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is updated locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been updated.");
}

// Ensures that the profile is saved to Account after submitting the form.
- (void)testUserData_AccountSave {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  [self fillPresidentProfileAndShowSaveModal];

  // Verify that the address badge is displayed.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBadgeButtonSaveAddressProfileAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];

  id<GREYMatcher> footerMatcher = grey_text(
      l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER,
                              base::UTF8ToUTF16(kEmail)));

  [[EarlGrey selectElementWithMatcher:footerMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Test that the profile can be saved for the edge case where the address form
// is hosted in a frame and is submitted there via XHR - when autofill across
// iframes is enabled.
- (void)testUserData_AccountSave_AutofillAcrossIframe_XHR {
  // Sign-in so the profile can be saved into the account.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Trigger the save infobar via XHR submission in the child frame.
  [self triggerSaveInfobarViaXHRSubmission];

  // Accept the banner to save the profile.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Ensures that the profile is saved to Account after submitting and editing the
// form.
- (void)testUserData_AccountEdit {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  [self fillPresidentProfileAndShowSaveModal];

  // Edit the profile.
  [[EarlGrey selectElementWithMatcher:ModalEditButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(@"City")]
      performAction:grey_replaceText(@"New York")];

  id<GREYMatcher> footerMatcher = grey_text(
      l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER,
                              base::UTF8ToUTF16(kEmail)));

  [[EarlGrey selectElementWithMatcher:footerMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Ensures that if a local profile is filled in a form and submitted, the user
// is asked for a migration prompt and the profile is moved to the Account.
- (void)testUserData_MigrationToAccount {
  [AutofillAppInterface clearProfilesStore];

  // Store one local address.
  [AutofillAppInterface saveExampleProfile];

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kProfileForm)];

  [self focusOnNameAndAutofill];

  // Submit the form.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementSubmit)];

  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Accept the banner.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  id<GREYMatcher> footerMatcher = grey_text(l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_ADDRESS_MIGRATE_IN_ACCOUNT_FOOTER,
      base::UTF8ToUTF16(kEmail)));
  // Check if there is footer suggesting it's a migration prompt.
  [[EarlGrey selectElementWithMatcher:footerMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Confirm to migrate the profile.
  [[EarlGrey selectElementWithMatcher:ModalMigrationButtonMatcher()]
      performAction:grey_tap()];

  // Ensure the count of profiles saved remains unchanged.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Tests that the user can edit a field in the edit via in the save address
// flow.
- (void)testUserData_LocalEditViaBottomSheet {
  // Fill and submit the form.
  [self fillPresidentProfileAndShowSaveModal];

  // Edit the profile.
  [[EarlGrey selectElementWithMatcher:ModalEditButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(@"City")]
      performAction:grey_replaceText(@"New York")];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              l10n_util::GetNSString(
                                                  IDS_IOS_AUTOFILL_COUNTRY)),
                                          grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];

  // Focus the search bar.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];

  // Verify the scrim is visible when search bar is focused but not typed in.
  [[EarlGrey selectElementWithMatcher:SearchBarScrim()]
      assertWithMatcher:grey_notNil()];

  // Verify the cancel button is visible and unfocuses search bar when tapped.
  [ChromeEarlGreyUI clearAndDismissSearchBar];

  // Verify countries are searchable using their name in the current
  // locale.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(@"Germany")];

  // Verify that scrim is not visible anymore.
  [[EarlGrey selectElementWithMatcher:SearchBarScrim()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:CountryEntry(@"Germany")]
      performAction:grey_tap()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");
}

// Tests that the bottom sheet to edit address is just hidden on Cancel.
- (void)testUserData_LocalHideBottomSheetOnCancel {
  // Fill and submit the form.
  [self fillPresidentProfileAndShowSaveModal];

  // Edit the profile.
  [[EarlGrey selectElementWithMatcher:ModalEditButtonMatcher()]
      performAction:grey_tap()];

  // Tap "Cancel"
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kEditProfileBottomSheetCancelButton),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];

  // Open modal by selecting the badge that shouldn't be accepted.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBadgeButtonSaveAddressProfileAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];
}

// Tests that the save address flow is still working correctly when the address
// badge is removed.
- (void)FLAKY_testSaveWithoutBadge {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  [self fillPresidentProfileAndShowSaveModal];

  // Verify that the address badge is not displayed.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBadgeButtonSaveAddressProfileAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  id<GREYMatcher> footerMatcher = grey_text(
      l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER,
                              base::UTF8ToUTF16(kEmail)));

  [[EarlGrey selectElementWithMatcher:footerMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Tests the sticky address prompt journey where the prompt remains there when
// navigating without an explicit user gesture, and then the prompt is dismissed
// when navigating with a user gesture. Test with the address save prompt but
// the type of address prompt doesn't matter in this test case.
- (void)testStickySavePromptJourney {
  [self fillPresidentProfileAndShowSaveInfobar];

  {
    // Reloading page from script shouldn't dismiss the infobar.
    NSString* script = @"location.reload();";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }
  {
    // Assigning url from script to the page aka open an url shouldn't dismiss
    // the infobar.
    NSString* script = @"window.location.assign(window.location.href);";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }
  {
    // Pushing new history entry without reloading content shouldn't dismiss the
    // infobar.
    NSString* script = @"history.pushState({}, '', 'destination2.html');";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }
  {
    // Replacing history entry without reloading content shouldn't dismiss the
    // infobar.
    NSString* script = @"history.replaceState({}, '', 'destination3.html');";
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
  }

  // Wait some time for things to settle.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(200));

  // Verify that the prompt is still there after the non-user initiated
  // navigations.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kInfobarBannerViewIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Navigate with an emulated user gesture and verify that dismisses the
  // prompt.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kProfileForm)];
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];
}

// Tests that there is an alert shown if the user tries to dismiss an alert
// after they edited a field in the edit prompt without saving.
// Note that this test is defined above.
- (void)MAYBE_testEditBottomSheetAlertBySwipingDown {
  // TODO(crbug.com/377270834): Fix implementation on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test fails on iPad currently.");
  }

  // TODO(crbug.com/443713676): Re-enable the test.
#if !TARGET_OS_SIMULATOR
  if (base::ios::IsRunningOnIOS26OrLater()) {
    if (![ChromeEarlGrey isIPadIdiom]) {
      EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
    }
  }
#endif

  // Fill and submit the form.
  [self fillPresidentProfileAndShowSaveModal];

  // Edit the profile.
  [[EarlGrey selectElementWithMatcher:ModalEditButtonMatcher()]
      performAction:grey_tap()];

  // Replace city field value.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(@"City")]
      performAction:grey_replaceText(@"New York")];

  // Swipe down the sheet.
  [[EarlGrey selectElementWithMatcher:EditProfileBottomSheet()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  id<GREYMatcher> keepEditingAlert = grey_text(
      l10n_util::GetNSString(IDS_IOS_VIEW_CONTROLLER_DISMISS_CANCEL_CHANGES));
  // Ensure the error alert is shown.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:keepEditingAlert];

  // Keep editing.
  [[EarlGrey selectElementWithMatcher:keepEditingAlert]
      performAction:grey_tap()];

  // Swipe down the sheet again.
  [[EarlGrey selectElementWithMatcher:EditProfileBottomSheet()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the save changes button exists.
  id<GREYMatcher> saveChangesAlert = grey_text(
      l10n_util::GetNSString(IDS_IOS_VIEW_CONTROLLER_DISMISS_SAVE_CHANGES));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:saveChangesAlert];

  [[EarlGrey selectElementWithMatcher:saveChangesAlert]
      performAction:grey_tap()];
}

// Tests that the 'Save' button is only enabled when all the required fields are
// filled.
// TODO(crbug.com/407573862): Re-enable after the test is fixed for
// ios-fieldtrial-rel.
- (void)DISABLED_testSaveButtonEnabledStateDependingOnRequiredFields {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey waitForSyncTransportStateActiveWithTimeout:base::Seconds(10)];

  // Fill and submit the form.
  [self fillPresidentProfileAndShowSaveModal];

  // Edit the profile.
  [[EarlGrey selectElementWithMatcher:ModalEditButtonMatcher()]
      performAction:grey_tap()];

  // Ensure the 'Save' button is initially enabled.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      assertWithMatcher:grey_enabled()];

  NSString* streetAddressLabel = base::SysUTF8ToNSString(
      autofill::FieldTypeToDeveloperRepresentationString(
          autofill::ADDRESS_HOME_STREET_ADDRESS));

  // Empty the street address field, which is required.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(streetAddressLabel)]
      performAction:grey_replaceText(@"")];

  // Scroll down to show the 'Save' button.
  [[EarlGrey selectElementWithMatcher:EditProfileBottomSheet()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Ensure the 'Save' button is disabled.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Re-fill the street address field.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(streetAddressLabel)]
      performAction:grey_replaceText(@"Street")];

  // Ensure the 'Save' button is enabled.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      assertWithMatcher:grey_enabled()];

  // Sign out.
  [SigninEarlGrey signOut];
}

// Tests that submission is detected hence the infobar is displayed when the
// "form" event behind the submission is `defaultPrevented` while the
// corresponding feature allows it.
- (void)testSubmissionDetection_defaultPrevented_whenAllowed {
  // Sign-in so the profile can be saved into the account.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Submit the form with `defaultPrevented` not considered.
  FullAddressFormPageParams params{.default_prevented = true, .redirect = true};
  [self loadAndSubmitFullAddressFormWithParams:params];

  // Wait on the infobar to be displayed after submission.
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Accept the banner to save the profile.
  [[EarlGrey selectElementWithMatcher:BannerButtonMatcher()]
      performAction:grey_tap()];

  // Wait for the save profile dialog to appear.
  [ChromeEarlGrey waitForMatcher:ModalButtonMatcher()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:ModalButtonMatcher()]
      performAction:grey_tap()];

  // Ensure profile is saved.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [SigninEarlGrey signOut];
}

// Tests that multiple submissions on the same form are deduped when deduping is
// enabled where only one submission per form element is allowed when.
- (void)testSubmissionDetectionWithDeduping {
  // Submit the form with `defaultPrevented` not considered and without
  // redirecting so the same form can be submitted multiple time.
  FullAddressFormPageParams params{.default_prevented = true,
                                   .redirect = false};
  [self loadAndSubmitFullAddressFormWithParams:params];

  // Wait on the infobar to be displayed after submission, meaning that
  // submission was detected.
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:YES];

  // Spam submissions.
  for (int i = 0; i < 5; ++i) {
    [ChromeEarlGrey tapWebStateElementWithID:@"submit-button"];
  }

  // Wait some time so the hypothetical form submission messages would have been
  // sent over to the browser by then.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(200));

  // Verify that only one submission was actually recorded despite triggering
  // multiple submissions on the same form.
  chrome_test_util::GREYAssertErrorNil([MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"Autofill.iOS.FormSubmission.OutcomeV2"]);
}

// Tests that the submission errors that occur in the renderer are reported to
// the browser.
- (void)testSubmissionErrorReporting_Enabled {
  // Inject a bug that will trigger error when handling the form submission in
  // the renderer.
  constexpr char kInjectedBug[] = R"(
    // Swizzle autofillSubmissionData() with an erroring function.
    gcrweb.gCrWeb.fill.autofillSubmissionData = function() {
      throw new Error("Oh no, something bad happened!");
    };
    // This is to give a return value to make the thing handling the JS
    // execution happy.
    true
  )";

  // Load page without submitting the form.
  [self loadFullAddressFormWithParams:{}];

  // Inject the bug in the submission handler so it triggers an error that will
  // be reported to the browser.
  [ChromeEarlGrey
      evaluateJavaScriptInIsolatedWorldForSideEffect:base::SysUTF8ToNSString(
                                                         kInjectedBug)];

  // Now that the submission handler is buggy, submit the form to trigger the
  // error.
  [ChromeEarlGrey tapWebStateElementWithID:@"submit-button"];

  // Verify that no infobar is displayed when there is a submission error.
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  // Verify that the submission error was reported and recorded.
  GREYAssertTrue(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::Milliseconds(200),
          ^{
            NSError* error = [MetricsAppInterface
                expectUniqueSampleWithCount:1
                                  forBucket:kRendererErrorHistogramBucket
                               forHistogram:
                                   @"Autofill.iOS.FormSubmission.OutcomeV2"];
            return error == nil;
          }),
      @"Timed out waiting for the submission error uma record.");
}

// Tests that the submission errors that occur in the renderer are not reported
// to the browser when the feature is disabled.
- (void)testSubmissionErrorReporting_Disabled {
  // Inject a bug that will trigger error when handling the form submission in
  // the renderer.
  constexpr char kInjectedBug[] = R"(
    // Swizzle autofillSubmissionData() with an erroring function.
    gcrweb.gCrWeb.fill.autofillSubmissionData = function() {
      throw new Error("Oh no, something bad happened!");
    };
    // This is to give a return value to make the thing handling the JS
    // execution happy.
    true
  )";

  // Load page without submitting the form.
  [self loadFullAddressFormWithParams:{}];

  // Inject the bug in the submission handler so it triggers an error that will
  // be reported to the browser.
  [ChromeEarlGrey
      evaluateJavaScriptInIsolatedWorldForSideEffect:base::SysUTF8ToNSString(
                                                         kInjectedBug)];

  // Now that the submission handler is buggy, submit the form to trigger the
  // error.
  [ChromeEarlGrey tapWebStateElementWithID:@"submit-button"];

  // Verify for some time that no infobar is displayed when there is a
  // submission error.
  [InfobarEarlGreyUI waitUntilInfobarBannerVisibleOrTimeout:NO];

  // Verify that no submission error was not reported and recorded. At this
  // point there should have been enough time to hypothetically handle the
  // submit event if there was no error.
  chrome_test_util::GREYAssertErrorNil([MetricsAppInterface
      expectTotalCount:0
          forHistogram:@"Autofill.iOS.FormSubmission.OutcomeV2"]);
}

// Tests submission count reporting with the scheduled task for the 2 types of
// form submission, regular and programmatic.
// TODO(crbug.com/428189566): Re-enable after the test is fixed for
// ios-fieldtrial-rel.
- (void)DISABLED_testSubmissionCountReporting_ScheduledTask {
  // Load the page without submitting the form.
  [self loadFullAddressFormWithParams:{.default_prevented = true,
                                       .multiple_submissions = true}];

  // Submit the form which will trigger 4 extra submissions consisting of 3
  // button click submissions and 1 `form.submit()` programmatic submission,
  // for a total of 5 submissions.
  [ChromeEarlGrey tapWebStateElementWithID:@"submit-button"];

  // Navigate back so the scheduled reporting task can be completed which
  // requires the frame to be "active".
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:"Address Form Test Page"];

  // Verify that that all the form click submissions were recorded. Wait enough
  // time for the report to be sent from the renderer. The reporting period is 2
  // seconds.
  GREYAssertTrue(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::Seconds(3),
          ^{
            // Expect 5 submissions in total, 4 regulars and 1 programmatic.
            NSError* error = [MetricsAppInterface
                 expectCount:4
                   forBucket:static_cast<int>(CountedSubmissionType::kHtmlEvent)
                forHistogram:
                    @"Autofill.iOS.FormActivity."
                    @"SubmissionDetectedBeforeProcessing.PerType.FromAll"];
            error = [MetricsAppInterface
                 expectCount:1
                   forBucket:static_cast<int>(
                                 CountedSubmissionType::kProgrammatic)
                forHistogram:
                    @"Autofill.iOS.FormActivity."
                    @"SubmissionDetectedBeforeProcessing.PerType.FromAll"];
            // Expect 2  regular submissions from the instant reports. Which
            // is the maximal number allowed by the quotas. Regular submissions
            // are handled in the isolated world.
            error = [MetricsAppInterface
                 expectCount:2
                   forBucket:static_cast<int>(CountedSubmissionType::kHtmlEvent)
                forHistogram:
                    @"Autofill.iOS.FormActivity."
                    @"SubmissionDetectedBeforeProcessing.PerType.FromInstant"];
            // Expect 1  programmatic submission from the instant reports. This
            // can bust the limit of 2 because the content world quotas are used
            // for the programmatic submission while the rest of Autofill is in
            // the isolated world (which includes the detection of regular click
            // submissions). Each world has its own quotas basically.
            error = [MetricsAppInterface
                 expectCount:1
                   forBucket:static_cast<int>(
                                 CountedSubmissionType::kProgrammatic)
                forHistogram:
                    @"Autofill.iOS.FormActivity."
                    @"SubmissionDetectedBeforeProcessing.PerType.FromInstant"];
            // Expect 2 regular submissions from the scheduled reports as all
            // the quotas were used for the 2 first reports.
            error = [MetricsAppInterface
                 expectCount:2
                   forBucket:static_cast<int>(CountedSubmissionType::kHtmlEvent)
                forHistogram:@"Autofill.iOS.FormActivity."
                             @"SubmissionDetectedBeforeProcessing.PerType."
                             @"FromScheduledTask"];
            // Expect 4 batches of reports, 3 from the instant reports (1
            // report per batch) and 1 batch with the 2 reports that were
            // reported by the scheduled task.
            error = [MetricsAppInterface
                 expectCount:3
                   forBucket:1
                forHistogram:@"Autofill.iOS.FormActivity."
                             @"SubmissionDetectedBeforeProcessing.BatchSize"];
            error = [MetricsAppInterface
                 expectCount:1
                   forBucket:2
                forHistogram:@"Autofill.iOS.FormActivity."
                             @"SubmissionDetectedBeforeProcessing.BatchSize"];
            return error == nil;
          }),
      @"Timed out waiting for the form submission metrics.");
}

// Tests submission count reporting when unloading a page.
// TODO(crbug.com/428189566): Re-enable after the test is fixed for
// ios-fieldtrial-rel.
- (void)DISABLED_testSubmissionCountReporting_UnloadPage {
  // Load page without submitting the form.
  [self loadFullAddressFormWithParams:{.default_prevented = true,
                                       .multiple_submissions_skip_programmatic =
                                           true}];

  // Submit the form which will trigger 4 extra submissions consisting of 3
  // button click submissions and 1 `form.submit()` programmatic submission,
  // for a total of 5 submissions.
  [ChromeEarlGrey tapWebStateElementWithID:@"submit-button"];

  // Reload the page content which triggers reporting.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:"Address Form Test Page"];

  // Verify that that all the submissions were reported. No need for a long
  // timeout as the report was sent when reloading the page so all the metrics
  // should have been recorded at this point.
  GREYAssertTrue(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::Milliseconds(200),
          ^{
            // Expect 4 regular submission in total.
            NSError* error = [MetricsAppInterface
                 expectCount:4
                   forBucket:static_cast<int>(CountedSubmissionType::kHtmlEvent)
                forHistogram:
                    @"Autofill.iOS.FormActivity."
                    @"SubmissionDetectedBeforeProcessing.PerType.FromAll"];
            // Expect 2  regular submissions from the instant reports. Which
            // is the maximal number allowed by the quotas. Regular submissions
            // are handled in the isolated world.
            error = [MetricsAppInterface
                 expectCount:2
                   forBucket:static_cast<int>(CountedSubmissionType::kHtmlEvent)
                forHistogram:
                    @"Autofill.iOS.FormActivity."
                    @"SubmissionDetectedBeforeProcessing.PerType.FromInstant"];
            // Expect 2 regular submissions from the report triggered by
            // unloading the page as all the quotas were used for the 2 first
            // reports.
            error = [MetricsAppInterface
                 expectCount:2
                   forBucket:static_cast<int>(CountedSubmissionType::kHtmlEvent)
                forHistogram:@"Autofill.iOS.FormActivity."
                             @"SubmissionDetectedBeforeProcessing.PerType."
                             @"FromUnloadPage"];
            // Expect 4 batches of reports, 2 from the instant reports (1
            // report per batch) and 1 batch with the 2 reports that were
            // reported when unloading the page.
            error = [MetricsAppInterface
                 expectCount:2
                   forBucket:1
                forHistogram:@"Autofill.iOS.FormActivity."
                             @"SubmissionDetectedBeforeProcessing.BatchSize"];
            error = [MetricsAppInterface
                 expectCount:1
                   forBucket:2
                forHistogram:@"Autofill.iOS.FormActivity."
                             @"SubmissionDetectedBeforeProcessing.BatchSize"];
            return error == nil;
          }),
      @"Timed out waiting for the form submission metrics.");
}

@end

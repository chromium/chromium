// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string_view>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/common/autofill_debug_features.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/personal_context/core/personal_context_debug_features.h"
#import "components/personal_context/core/personal_context_features.h"
#import "components/personal_context/core/personal_context_prefs.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_egtest_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using autofill::FocusWebElementWithId;
using base::test::ios::kWaitForActionTimeout;
using chrome_test_util::ContextMenuItemWithAccessibilityLabelId;

namespace {

constexpr char kFormPassportNumber[] = "passportNumber";
constexpr char kFormPassportOwnerName[] = "passportOwnerName";
constexpr char kFormPassportCountryCode[] = "passportCountryCode";
constexpr char kFormPassportIssueDate[] = "passportIssueDate";
constexpr char kFormPassportExpirationDate[] = "passportExpirationDate";

NSString* const kPassportEntityType = @"Passport";
NSString* const kPassportNumber = @"LR1234567";
NSString* const kPassportOwnerName = @"Pippi Långstrump";
NSString* const kPassportCountry = @"Sweden";
NSString* const kPassportIssueDate = @"2010-09-01";
NSString* const kPassportExpirationDate = @"2019-08-30";

// Form and field signature overrides for `/passport_form.html`, generated via
// `autofill::CalculateFormSignature` and
// `autofill::CalculateFieldSignatureForField`.
constexpr char kPassportOverrideParam[] = "4641858130178050027_57480302_169-"
                                          "4641858130178050027_1260605411_7-"
                                          "4641858130178050027_2806930637_170-"
                                          "4641858130178050027_1520818241_172-"
                                          "4641858130178050027_2813548763_171";

// Returns the expected accessibility label for the Sian (Personal Context)
// passport suggestion chip in the keyboard accessory bar.
NSString* PersonalContextPassportSuggestionAccessibilityLabel() {
  NSString* obfuscated_number =
      [AutofillAppInterface obfuscatedValue:kPassportNumber
                        visibleSuffixLength:4];
  NSString* base_label =
      [NSString stringWithFormat:@"%@, %@ · %@", obfuscated_number,
                                 kPassportEntityType, kPassportOwnerName];
  return l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION, base::SysNSStringToUTF16(base_label),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_SUGGESTED_BY_GEMINI));
}

// Matcher for the "Finding info with Gemini" loading suggestion chip.
id<GREYMatcher> FetchingAmbientDataChipMatcher() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_AUTOFILL_AI_FETCHING_AMBIENT_DATA)),
                    grey_ancestor(grey_accessibilityID(
                        kFormInputAccessoryViewAccessibilityID)),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the Sian (Personal Context) passport suggestion chip.
id<GREYMatcher> PersonalContextPassportChipMatcher() {
  return grey_allOf(grey_accessibilityLabel(
                        PersonalContextPassportSuggestionAccessibilityLabel()),
                    grey_ancestor(grey_accessibilityID(
                        kFormInputAccessoryViewAccessibilityID)),
                    grey_sufficientlyVisible(), nil);
}

// Signs in an eligible user and enables Ambient Autofill and Gemini consent
// preferences.
void SetUpEligibleUser() {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fake_identity
                 withCapabilities:@{
                   @(kCanUseModelExecutionFeaturesName) : @YES,
                   @(kCanUseGeminiInChromeCapabilityName) : @YES,
                 }];
  [SigninEarlGrey signinWithFakeIdentity:fake_identity];
  [ChromeEarlGrey
      setBoolValue:YES
       forUserPref:personal_context::prefs::
                       kPersonalContextInAutofillSettingsToggleStatus];
  [ChromeEarlGrey
      setBoolValue:NO
       forUserPref:personal_context::prefs::
                       kPersonalContextAmbientAutofillNoticeShouldBeShown];
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kIOSBwgConsent];
  [ChromeEarlGrey setIntegerValue:0 forUserPref:prefs::kGeminiEnabledByPolicy];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:prefs::kAIHubEligibilityTriggered];
}

}  // namespace

// Integration tests for Ambient Autofill with Personal Context (Sian) entities.
@interface AmbientAutofillTestCase : ChromeTestCase
@end

@implementation AmbientAutofillTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      autofill::features::kAutofillAmbientAutofill);
  config.features_enabled.push_back(
      autofill::features::kAutofillAmbientAutofillSuppression);
  config.features_enabled.push_back(
      autofill::features::kAutofillAiWithDataSchema);
  config.features_enabled.push_back(
      autofill::features::debug::kAutofillAiForceOptIn);
  config.features_enabled.push_back(
      autofill::features::kAutofillAiAlwaysTriggerServerModel);
  config.features_enabled_and_params.push_back(
      {autofill::features::debug::kAutofillOverridePredictions,
       {{"spec", kPassportOverrideParam}}});
  config.features_enabled.push_back(
      personal_context::features::kPersonalContext);
  config.features_enabled.push_back(
      personal_context::features::debug::
          kAutofillAmbientAutofillSkipEligibilityChecks);
  return config;
}

- (void)setUp {
  [super setUp];

  if (!self.testServer->Started()) {
    GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  }

  SetUpEligibleUser();

  [self addTeardownBlock:^{
    [AutofillAppInterface clearPersonalContextPassportEntity];
  }];
}

#pragma mark - Tests

// Tests the full Sian passport flow: showing the "Finding info with Gemini"
// loading chip while prefetch is in flight, transitioning to the Sian passport
// suggestion chip once prefetch completes, and filling all passport form fields
// with the unmasked Sian passport when tapped.
- (void)testFillPersonalContextPassportForm {
  [AutofillAppInterface startPersonalContextPassportPrefetch];
  [self loadPassportPage];
  [self focusPassportNumberField];

  // Verify the "Finding info with Gemini" loading chip appears while prefetch
  // is pending.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:FetchingAmbientDataChipMatcher()];

  // Complete the prefetch and inject the Sian passport entity.
  GREYAssertTrue([AutofillAppInterface savePersonalContextPassportEntity],
                 @"Failed to save Personal Context passport entity.");

  // Verify the Sian passport suggestion chip replaces the loading chip.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:PersonalContextPassportChipMatcher()];
  [[EarlGrey selectElementWithMatcher:FetchingAmbientDataChipMatcher()]
      assertWithMatcher:grey_nil()];

  // Tap the Sian passport suggestion chip and verify all passport fields are
  // filled with the unmasked values.
  [[EarlGrey selectElementWithMatcher:PersonalContextPassportChipMatcher()]
      performAction:grey_tap()];

  [self verifyFieldWithIDHasBeenFilled:kFormPassportNumber
                                 value:kPassportNumber];
  [self verifyFieldWithIDHasBeenFilled:kFormPassportOwnerName
                                 value:kPassportOwnerName];
  [self verifyFieldWithIDHasBeenFilled:kFormPassportCountryCode
                                 value:kPassportCountry];
  [self verifyFieldWithIDHasBeenFilled:kFormPassportIssueDate
                                 value:kPassportIssueDate];
  [self verifyFieldWithIDHasBeenFilled:kFormPassportExpirationDate
                                 value:kPassportExpirationDate];
}

// Tests that long-pressing a Sian passport suggestion chip displays the
// Personal Context context menu with "Open settings", "View sources", and
// "Remove" actions.
- (void)testPersonalContextPassportContextMenu {
  GREYAssertTrue([AutofillAppInterface savePersonalContextPassportEntity],
                 @"Failed to save Personal Context passport entity.");
  [self loadPassportPage];
  [self focusPassportNumberField];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:PersonalContextPassportChipMatcher()];
  [[EarlGrey selectElementWithMatcher:PersonalContextPassportChipMatcher()]
      performAction:grey_longPress()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      ContextMenuItemWithAccessibilityLabelId(
                          IDS_IOS_CONTEXT_MENU_OPEN_SETTINGS)];
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_AUTOFILL_AI_VIEW_SOURCES)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ContextMenuItemWithAccessibilityLabelId(
                                          IDS_IOS_AUTOFILL_AI_REMOVE_ACTION)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Private

// Loads the passport form test page and waits for server prediction overrides
// to be applied to the cached form.
- (void)loadPassportPage {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/passport_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Passport"];
  // Local heuristics classify `/passport_form.html` fields as `UNKNOWN_TYPE`;
  // `PASSPORT_*` types are only applied asynchronously via
  // `kAutofillOverridePredictions` in
  // `AutofillManager::OnLoadedServerPredictions`. Focusing the field before
  // predictions arrive causes `AutofillAiManager` to produce no suggestions.
  GREYAssertTrue([AutofillAppInterface waitForServerPredictionsInMainFrame],
                 @"Server predictions were not applied to cached forms.");
}

// Scrolls to and focuses the passport number input field, then waits for the
// keyboard accessory view to appear.
- (void)focusPassportNumberField {
  // `/passport_form.html` has no `<meta name="viewport">`, so on iOS 27
  // `WKWebView` may still be settling its initial zoom scale and `contentInset`
  // right after page load (especially following a Tab Grid-to-BVC transition),
  // causing a single coordinate tap to miss `#passportNumber`.
  // `FocusWebElementWithId` centers the element and retries with freshly
  // computed coordinates until `document.activeElement` matches.
  FocusWebElementWithId(kFormPassportNumber);
  // Wait for the keyboard accessory bar rather than `-waitForKeyboardToAppear`,
  // since these tests interact with accessory suggestions and the accessory
  // view can be shown even when the software keyboard stays hidden.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kFormInputAccessoryViewAccessibilityID)
                                  timeout:kWaitForActionTimeout];
}

// Verifies that the HTML input field with `fieldID` has been filled with
// `value`.
- (void)verifyFieldWithIDHasBeenFilled:(std::string_view)fieldID
                                 value:(NSString*)value {
  NSString* condition = [NSString
      stringWithFormat:@"window.document.getElementById('%@').value === '%@'",
                       base::SysUTF8ToNSString(fieldID), value];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

@end

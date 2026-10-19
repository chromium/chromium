// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <optional>
#import <string>
#import <string_view>

#import "base/path_service.h"
#import "components/autofill/core/common/autofill_debug_features.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/personal_context/core/personal_context_debug_features.h"
#import "components/personal_context/core/personal_context_prefs.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/autofill/atmemory/test/at_memory_test_util.h"
#import "ios/chrome/browser/autofill/manual_fill/test/manual_fill_matchers.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {
constexpr char kVehicleFormPageURL[] = "/vehicle_form.html";
constexpr char kVehicleMakeFieldID[] = "vehicleMake";
constexpr char kMakeFieldText[] = "Make:";
constexpr char kHttpServerFilesDirectory[] =
    "ios/testing/data/http_server_files/";
constexpr char kMockPersonalContextVehicleMakeResultType[] = "14";
constexpr CGFloat kAIDisclosureLinkTapXPercentage = 0.15;
constexpr CGFloat kAIDisclosureLinkTapYPercentage = 0.75;

// Override parameter used to simulate server predictions for
// `kVehicleFormPageURL`.
constexpr std::string_view kVehicleOverrideParam =
    "2031728383716307484_1092605073_178-"
    "2031728383716307484_2435123338_179-"
    "2031728383716307484_3040116536_185-"
    "2031728383716307484_1294171443_7-"
    "2031728383716307484_3811017579_176-"
    "2031728383716307484_2296829005_186-"
    "2031728383716307484_3849212670_177";

// Element IDs for SSL warning bypass.
NSString* const kDetailsButtonID = @"details-button";
NSString* const kProceedLinkID = @"proceed-link";

// Assertion failure messages.
NSString* const kHttpsServerStartFailureMessage =
    @"HTTPS server did not start.";

// Search queries.
NSString* const kCarSearchQuery = @"car";
NSString* const kCarSearchQuerySuffix = @"s";
NSString* const kCarsSearchQuery = @"cars";
NSString* const kNoDataSearchQuery = @"flight";

// Default entity GUID used by `autofill::test::GetVehicleEntityInstance()`.
NSString* const kVehicleEntityID = @"00000000-0000-4000-8000-200000000000";

// Test entity data and labels.
NSString* const kVehicleMakeBMW = @"BMW";
NSString* const kVehicleMakeDoubleFilledBMW = @"BMWBMW";
NSString* const kVehicleModelSeries2 = @"Series 2";

// JavaScript format strings.
NSString* const kFieldValueCheckScriptFormat =
    @"document.getElementById('%s')?.value === '%@'";

// Starts the HTTPS test server serving test HTML files.
void SetUpHTTPSServer(
    std::optional<net::test_server::EmbeddedTestServer>& server) {
  server.emplace(net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  server->ServeFilesFromDirectory(
      base::PathService::CheckedGet(base::DIR_ASSETS)
          .AppendASCII(kHttpServerFilesDirectory));
  net::test_server::RegisterDefaultHandlers(&server.value());
  GREYAssertTrue(server->Start(), kHttpsServerStartFailureMessage);
}

// Signs in a fake identity with Gemini and model execution capabilities and
// sets eligible preferences for AtMemory.
void SetUpAtMemoryEligibleUser() {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kCanUseModelExecutionFeaturesName) : @YES,
                   @(kCanUseGeminiInChromeCapabilityName) : @YES,
                 }];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kIOSBwgConsent];
  [ChromeEarlGrey setIntegerValue:0 forUserPref:prefs::kGeminiEnabledByPolicy];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:prefs::kAIHubEligibilityTriggered];
}

// Proceeds past the SSL warning interstitial shown for the HTTPS test server.
void BypassSSLWarning() {
  [ChromeEarlGrey waitForJavaScriptCondition:
                      @"document.getElementById('details-button') !== null"];
  [ChromeEarlGrey tapWebStateElementWithID:kDetailsButtonID];
  [ChromeEarlGrey tapWebStateElementWithID:kProceedLinkID];
}

// Loads the vehicle form page on the HTTPS test server.
void LoadVehicleFormPage(net::test_server::EmbeddedTestServer* test_server) {
  [ChromeEarlGrey loadURL:test_server->GetURL(kVehicleFormPageURL)];
  BypassSSLWarning();
  [ChromeEarlGrey waitForWebStateContainingText:kMakeFieldText];
  GREYAssertTrue([AutofillAppInterface waitForFormToBeCachedInMainFrame],
                 @"Forms were not cached in main frame.");
}

// Focuses the `kVehicleMakeFieldID` input on the page and taps the AtMemory
// button in the keyboard accessory manual fill to open the AtMemory bottom
// sheet.
void OpenAtMemoryForVehicleMakeField() {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kVehicleMakeFieldID)];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil atMemoryButton]];
  [[EarlGrey selectElementWithMatcher:[AtMemoryTestUtil atMemoryButton]]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil bottomSheetTitle]];
}

// Verifies that the web input field with `field_id` has been filled with
// `expected_value`.
void VerifyFieldHasBeenFilled(const char* field_id, NSString* expected_value) {
  NSString* condition = [NSString
      stringWithFormat:kFieldValueCheckScriptFormat, field_id, expected_value];
  [ChromeEarlGrey waitForJavaScriptCondition:condition];
}

// Types `query` into the AtMemory search bar and taps the search prompt cell to
// submit the search.
void AtMemorySearchWithQuery(NSString* query) {
  [[EarlGrey selectElementWithMatcher:[AtMemoryTestUtil searchButton]]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:[AtMemoryTestUtil searchBar]]
      performAction:grey_typeText(query)];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil
                                              searchPromptCellWithQuery:query]];
  [[EarlGrey selectElementWithMatcher:[AtMemoryTestUtil
                                          searchPromptCellWithQuery:query]]
      performAction:grey_tap()];
}

// Opens the Suggestions from Gemini settings page via the inline notice link.
void OpenSuggestionsFromGeminiSettingsViaInlineNotice() {
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil inlineNoticeTitle]];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil
                                              inlineNoticeSettingsLink]];
  // As the settings link can be split across two lines, use a precise point tap
  // to avoid tapping empty whitespace in the multi-line bounding box.
  [[EarlGrey
      selectElementWithMatcher:[AtMemoryTestUtil inlineNoticeSettingsLink]]
      performAction:chrome_test_util::TapAtPointPercentage(0.95, 0.05)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          chrome_test_util::TableViewSwitchCell(
              kSuggestionsFromGeminiSwitchViewId, /*is_toggled_on=*/YES,
              /*is_enabled=*/YES)];
}

// Dismisses settings by tapping the "Done" button.
void DismissSettings() {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}
}  // namespace

// Test case for the AtMemory screen.
@interface AtMemoryTestCase : ChromeTestCase {
  std::optional<net::test_server::EmbeddedTestServer> _HTTPSServer;
}
@end

@implementation AtMemoryTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(autofill::features::kAutofillAtMemory);
  config.features_enabled.push_back(
      autofill::features::kAutofillAtMemorySearchStatefulness);
  config.features_enabled.push_back(
      autofill::features::kAutofillAtMemoryPreviouslyFilled);
  config.features_enabled.push_back(
      autofill::features::debug::kAtMemorySkipEnablementChecks);
  config.features_enabled.push_back(
      autofill::features::kAutofillAiWithDataSchema);
  // Mock query intent resolution to return `MemoryDataType::kVehicleMake`
  // (14).
  config.features_enabled_and_params.push_back(
      {personal_context::features::debug::kMockPersonalContextResult,
       {{{personal_context::features::debug::kMockPersonalContextResultTypeParam
              .name,
          kMockPersonalContextVehicleMakeResultType}}}});

  if ([self isRunningTest:@selector(testInlineNoticeAcknowledge)] ||
      [self isRunningTest:@selector(testInlineNoticeSettingsLink)] ||
      [self isRunningTest:@selector(
                              testDisableFeatureInSettingsDismissesAtMemory)]) {
    config.features_enabled.push_back(
        personal_context::features::debug::
            kAutofillAmbientAutofillSkipEligibilityChecks);
    config.features_enabled.push_back(
        personal_context::features::debug::
            kPersonalContextResetNoticePrefsOnStartup);
  }

  if ([self
          isRunningTest:@selector(
                            testShowsAtMemoryBottomSheetFromSuggestionStrip)]) {
    config.features_enabled.push_back(
        autofill::features::debug::kAutofillAiForceOptIn);
    config.features_enabled_and_params.push_back(
        {autofill::features::debug::kAutofillOverridePredictions,
         {{"spec", std::string(kVehicleOverrideParam)}}});
    config.features_enabled.push_back(
        autofill::features::kAutofillAiAlwaysTriggerServerModel);
  }
  return config;
}

- (void)setUp {
  [super setUp];
  SetUpHTTPSServer(_HTTPSServer);
  SetUpAtMemoryEligibleUser();
  LoadVehicleFormPage(&_HTTPSServer.value());
  if (![self
          isRunningTest:@selector(
                            testShowsAtMemoryBottomSheetFromSuggestionStrip)]) {
    OpenAtMemoryForVehicleMakeField();
  }
}

- (void)tearDownHelper {
  [AutofillAppInterface removeEntityWithUUID:kVehicleEntityID];
  [AutofillAppInterface setNetworkConnectionOffline:NO];
  _HTTPSServer.reset();
  // Restores the toggle turned off by
  // `testDisableFeatureInSettingsDismissesAtMemory`; only inline notice tests
  // reset this pref on startup.
  [ChromeEarlGrey
      clearUserPrefWithName:personal_context::prefs::
                                kPersonalContextInAutofillSettingsToggleStatus];
  [super tearDownHelper];
}

// Tests that tapping the magnifying glass spark icon in the keyboard accessory
// shows the AtMemory bottom sheet with the zero-state empty image.
- (void)testShowsAtMemoryBottomSheet {
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil emptyStateImage]];
}

// Tests that tapping the AtMemory suggestion chip in the keyboard suggestion
// scroll view opens the AtMemory bottom sheet.
- (void)testShowsAtMemoryBottomSheetFromSuggestionStrip {
  GREYAssertTrue([AutofillAppInterface saveVehicleEntity],
                 @"Failed to save vehicle entity.");

  // Focus on the vehicle make field to trigger suggestions.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kVehicleMakeFieldID)];

  // Scroll to the right of the keyboard accessory so that the AtMemory
  // suggestion chip is visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];

  // Tap the AtMemory suggestion chip in the suggestion strip.
  [[EarlGrey
      selectElementWithMatcher:[AtMemoryTestUtil suggestionStripAtMemoryButton]]
      performAction:grey_tap()];

  // Verify the AtMemory bottom sheet is visible.
  [[EarlGrey selectElementWithMatcher:[AtMemoryTestUtil bottomSheetTitle]]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that searching for a query with no matching data ("flight") displays
// the "No Data" error state.
- (void)testNoDataState {
  AtMemorySearchWithQuery(kNoDataSearchQuery);

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil noDataCell]];
}

// Tests that submitting a search query while offline displays the "No
// Connection" error state.
- (void)testNoConnectionState {
  [AutofillAppInterface setNetworkConnectionOffline:YES];
  AtMemorySearchWithQuery(kCarSearchQuery);

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil noConnectionCell]];
}

// Tests that typing in the search bar updates the search prompt cell and
// displays the AI disclosure footer.
- (void)testTypingState {
  [[EarlGrey selectElementWithMatcher:[AtMemoryTestUtil searchButton]]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:[AtMemoryTestUtil searchBar]]
      performAction:grey_typeText(kCarSearchQuery)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          [AtMemoryTestUtil searchPromptCellWithQuery:kCarSearchQuery]];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil
                                                          aiDisclosureFooter]];

  [[EarlGrey selectElementWithMatcher:[AtMemoryTestUtil searchBar]]
      performAction:grey_typeText(kCarSearchQuerySuffix)];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          [AtMemoryTestUtil searchPromptCellWithQuery:kCarsSearchQuery]];
}

// Tests that tapping the link in the AI disclosure footer while in the typing
// state opens the Help improve enhanced autofill settings page.
- (void)testAIDisclosureLink {
  [[EarlGrey selectElementWithMatcher:[AtMemoryTestUtil searchButton]]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:[AtMemoryTestUtil searchBar]]
      performAction:grey_typeText(kCarSearchQuery)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil
                                              aiDisclosureFooterLink]];
  // When the AI disclosure link wraps across multiple lines, a center tap
  // misses the link glyphs, so tap on the second line where the wrapped link
  // text starts. Otherwise, a standard center tap hits the single-line link.
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:[AtMemoryTestUtil aiDisclosureFooterLink]]
      assertWithMatcher:[AtMemoryTestUtil isMultilineLink]
                  error:&error];
  id<GREYAction> tapAction = error ? grey_tap()
                                   : chrome_test_util::TapAtPointPercentage(
                                         kAIDisclosureLinkTapXPercentage,
                                         kAIDisclosureLinkTapYPercentage);
  [[EarlGrey selectElementWithMatcher:[AtMemoryTestUtil aiDisclosureFooterLink]]
      performAction:tapAction];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil
                                              helpImproveSettingsPage]];
}

// Tests granular filling of a specific field from the entity details view.
- (void)testGranularFill {
  [AutofillAppInterface saveVehicleEntity];
  AtMemorySearchWithQuery(kCarSearchQuery);

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          [AtMemoryTestUtil searchResultCellWithTitle:kVehicleMakeBMW]];
  [[EarlGrey
      selectElementWithMatcher:
          [AtMemoryTestUtil infoButtonForSearchResultWithTitle:kVehicleMakeBMW]]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          [AtMemoryTestUtil chipButtonWithLabel:kVehicleModelSeries2]];
  [[EarlGrey
      selectElementWithMatcher:[AtMemoryTestUtil
                                   chipButtonWithLabel:kVehicleModelSeries2]]
      performAction:grey_tap()];

  VerifyFieldHasBeenFilled(kVehicleMakeFieldID, kVehicleModelSeries2);
}

// Tests that previously filled items appear in the zero state and can be
// selected to re-fill.
- (void)testRecentFills {
  [AutofillAppInterface saveVehicleEntity];
  AtMemorySearchWithQuery(kCarSearchQuery);

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          [AtMemoryTestUtil searchResultCellWithTitle:kVehicleMakeBMW]];
  [[EarlGrey
      selectElementWithMatcher:[AtMemoryTestUtil
                                   searchResultCellWithTitle:kVehicleMakeBMW]]
      performAction:grey_tap()];

  VerifyFieldHasBeenFilled(kVehicleMakeFieldID, kVehicleMakeBMW);

  OpenAtMemoryForVehicleMakeField();

  // Test filling with previously filled data works and appends to existing
  // text.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          [AtMemoryTestUtil searchResultCellWithTitle:kVehicleMakeBMW]];
  [[EarlGrey
      selectElementWithMatcher:[AtMemoryTestUtil
                                   searchResultCellWithTitle:kVehicleMakeBMW]]
      performAction:grey_tap()];

  VerifyFieldHasBeenFilled(kVehicleMakeFieldID, kVehicleMakeDoubleFilledBMW);
}

// Tests that the inline privacy notice is displayed and can be dismissed.
- (void)testInlineNoticeAcknowledge {
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil inlineNoticeTitle]];
  [[EarlGrey selectElementWithMatcher:[AtMemoryTestUtil inlineNoticeOKButton]]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:[AtMemoryTestUtil
                                                 inlineNoticeTitle]];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil emptyView]];
}

// Tests that tapping the "Manage settings" link in the inline privacy notice
// opens the Suggestions from Gemini settings page, and dismissing returns to
// the AtMemory screen with the notice dismissed.
- (void)testInlineNoticeSettingsLink {
  OpenSuggestionsFromGeminiSettingsViaInlineNotice();
  DismissSettings();

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil bottomSheetTitle]];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:[AtMemoryTestUtil
                                                 inlineNoticeTitle]];
}

// Tests that disabling the Suggestions from Gemini toggle in settings causes
// the AtMemory bottom sheet to automatically dismiss when settings is closed.
- (void)testDisableFeatureInSettingsDismissesAtMemory {
  OpenSuggestionsFromGeminiSettingsViaInlineNotice();

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSuggestionsFromGeminiSwitchViewId)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  DismissSettings();

  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:[AtMemoryTestUtil
                                                             bottomSheetTitle]];
}

@end

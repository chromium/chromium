// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <optional>

#import "base/path_service.h"
#import "components/autofill/core/common/autofill_debug_features.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/personal_context/core/personal_context_debug_features.h"
#import "components/personal_context/core/personal_context_prefs.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/autofill/atmemory/test/at_memory_test_util.h"
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

// Proceeds past the SSL warning interstitial shown for the HTTPS test server
// if present.
void BypassSSLWarning() {
  base::Value result = [ChromeEarlGrey
      evaluateJavaScript:@"document.getElementById('details-button') !== null"];
  if (result.is_bool() && result.GetBool()) {
    [ChromeEarlGrey tapWebStateElementWithID:kDetailsButtonID];
    [ChromeEarlGrey tapWebStateElementWithID:kProceedLinkID];
  }
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
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil searchBar]];
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

// TODO(crbug.com/570993873): Fix and re-enable these tests when compiled with
// iOS 27.1 SDK.
#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1
#define MAYBE_testInlineNoticeAcknowledge DISABLED_testInlineNoticeAcknowledge
#define MAYBE_testInlineNoticeSettingsLink DISABLED_testInlineNoticeSettingsLink
#define MAYBE_testDisableFeatureInSettingsDismissesAtMemory \
  DISABLED_testDisableFeatureInSettingsDismissesAtMemory
#else
#define MAYBE_testInlineNoticeAcknowledge testInlineNoticeAcknowledge
#define MAYBE_testInlineNoticeSettingsLink testInlineNoticeSettingsLink
#define MAYBE_testDisableFeatureInSettingsDismissesAtMemory \
  testDisableFeatureInSettingsDismissesAtMemory
#endif

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

  if ([self isRunningTest:@selector(MAYBE_testInlineNoticeAcknowledge)] ||
      [self isRunningTest:@selector(MAYBE_testInlineNoticeSettingsLink)] ||
      [self
          isRunningTest:
              @selector(MAYBE_testDisableFeatureInSettingsDismissesAtMemory)]) {
    config.features_enabled.push_back(
        personal_context::features::debug::
            kAutofillAmbientAutofillSkipEligibilityChecks);
    config.features_enabled.push_back(
        personal_context::features::debug::
            kPersonalContextResetNoticePrefsOnStartup);
  }
  return config;
}

- (void)setUp {
  [super setUp];
  SetUpHTTPSServer(_HTTPSServer);
  SetUpAtMemoryEligibleUser();
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey closeTabAtIndex:0];
  LoadVehicleFormPage(&_HTTPSServer.value());
  OpenAtMemoryForVehicleMakeField();
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
// TODO(crbug.com/570993873): Fix and re-enable this test when compiled with iOS
// 27.1 SDK.
#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1
#define MAYBE_testTypingState DISABLED_testTypingState
#else
#define MAYBE_testTypingState testTypingState
#endif
- (void)MAYBE_testTypingState {
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

// Tests granular filling of a specific field from the entity details view.
// TODO(crbug.com/570993873): Fix and re-enable this test when compiled with iOS
// 27.1 SDK.
#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1
#define MAYBE_testGranularFill DISABLED_testGranularFill
#else
#define MAYBE_testGranularFill testGranularFill
#endif
- (void)MAYBE_testGranularFill {
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
// TODO(crbug.com/570993873): Fix and re-enable this test when compiled with iOS
// 27.1 SDK.
#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1
#define MAYBE_testRecentFills DISABLED_testRecentFills
#else
#define MAYBE_testRecentFills testRecentFills
#endif
- (void)MAYBE_testRecentFills {
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
- (void)MAYBE_testInlineNoticeAcknowledge {
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
// the AtMemory screen with the notice still visible.
- (void)MAYBE_testInlineNoticeSettingsLink {
  OpenSuggestionsFromGeminiSettingsViaInlineNotice();
  DismissSettings();

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil searchBar]];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:[AtMemoryTestUtil inlineNoticeTitle]];
}

// Tests that disabling the Suggestions from Gemini toggle in settings causes
// the AtMemory bottom sheet to automatically dismiss when settings is closed.
- (void)MAYBE_testDisableFeatureInSettingsDismissesAtMemory {
  OpenSuggestionsFromGeminiSettingsViaInlineNotice();

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSuggestionsFromGeminiSwitchViewId)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  DismissSettings();

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:[AtMemoryTestUtil searchBar]];
}

@end

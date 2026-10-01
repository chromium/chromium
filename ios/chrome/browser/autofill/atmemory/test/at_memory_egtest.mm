// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <optional>

#import "base/path_service.h"
#import "components/autofill/core/common/autofill_debug_features.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/autofill/atmemory/test/at_memory_test_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
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

// Element IDs for SSL warning bypass.
NSString* const kDetailsButtonID = @"details-button";
NSString* const kProceedLinkID = @"proceed-link";

// Assertion failure messages.
NSString* const kHttpsServerStartFailureMessage =
    @"HTTPS server did not start.";

// Search queries.
NSString* const kCarSearchQuery = @"car";
NSString* const kNoDataSearchQuery = @"flight";

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
      autofill::features::debug::kAtMemorySkipEnablementChecks);
  config.features_enabled.push_back(
      autofill::features::kAutofillAiWithDataSchema);
  return config;
}

- (void)setUp {
  [super setUp];
  SetUpHTTPSServer(_HTTPSServer);
  SetUpAtMemoryEligibleUser();
  LoadVehicleFormPage(&_HTTPSServer.value());
  OpenAtMemoryForVehicleMakeField();
}

- (void)tearDownHelper {
  [AutofillAppInterface setNetworkConnectionOffline:NO];
  _HTTPSServer.reset();
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

@end

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/strcat.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_command_line.h"
#import "components/omnibox/browser/aim_eligibility_service_features.h"
#import "components/policy/core/common/policy_test_utils.h"
#import "components/policy/policy_constants.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/ntp_home_constant.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const char kPageLoadedString[] = "Page loaded!";
const char kPageURL[] = "/test-page.html";
const char kPageTitle[] = "Page title!";

// The passphrase for the fake sync server.
NSString* const kPassphrase = @"hello";

// The primary identity.
FakeSystemIdentity* const kPrimaryIdentity = [FakeSystemIdentity fakeIdentity1];

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kPageURL) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content("<html><head><title>" + std::string(kPageTitle) +
                             "</title></head><body>" +
                             std::string(kPageLoadedString) + "</body></html>");
  return std::move(http_response);
}

// Pauses until the history label has disappeared.  History should not show on
// incognito.
BOOL WaitForHistoryToDisappear() {
  return [[GREYCondition
      conditionWithName:@"Wait for history to disappear"
                  block:^BOOL {
                    NSError* error = nil;
                    NSString* history =
                        l10n_util::GetNSString(IDS_HISTORY_SHOW_HISTORY);
                    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                                            history)]
                        assertWithMatcher:grey_notVisible()
                                    error:&error];
                    return error == nil;
                  }]
      waitWithTimeout:base::test::ios::kWaitForUIElementTimeout.InSecondsF()];
}

// The possible visibility states of quick actions.
enum class QuickActionsVisibility {
  // All three quick actions are fully visible.
  kVisible = 0,
  // Quick asctions are visible without incognito.
  kVisibleWithoutIncognito = 1,
  // Quick actions are not visible.
  kNotVisible = 2,
};

// Verifies whether the quick action row respects the expected visibility.
void VerifyQuickActionVisibility(QuickActionsVisibility expected_visibility) {
  auto incognitoElement = [EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPIncognitoQuickActionIdentifier)];
  auto lensElement =
      [EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                             kNTPLensQuickActionIdentifier)];
  auto voiceSearchElement = [EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNTPVoiceSearchQuickActionIdentifier)];

  switch (expected_visibility) {
    case QuickActionsVisibility::kVisible:
      [incognitoElement assertWithMatcher:grey_sufficientlyVisible()];
      [lensElement assertWithMatcher:grey_sufficientlyVisible()];
      [voiceSearchElement assertWithMatcher:grey_sufficientlyVisible()];
      break;
    case QuickActionsVisibility::kVisibleWithoutIncognito:
      [incognitoElement assertWithMatcher:grey_notVisible()];
      [lensElement assertWithMatcher:grey_sufficientlyVisible()];
      [voiceSearchElement assertWithMatcher:grey_sufficientlyVisible()];
      break;
    case QuickActionsVisibility::kNotVisible:
      [incognitoElement assertWithMatcher:grey_notVisible()];
      [lensElement assertWithMatcher:grey_notVisible()];
      [voiceSearchElement assertWithMatcher:grey_notVisible()];
      break;
  }
}

void VerifyMIAButtonVisible(bool mia_button_visible) {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kNTPMIAIdentifier)]
      assertWithMatcher:mia_button_visible ? grey_sufficientlyVisible()
                                           : grey_notVisible()];
}

}  // namespace

@interface NewTabPageTestCase : ChromeTestCase
@property(nonatomic, assign) BOOL histogramTesterSet;
@end

@implementation NewTabPageTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  if ([self isRunningTest:@selector(DISABLED_testErrorBadge)]) {
    config.features_enabled.push_back(
        switches::kEnableErrorBadgeOnIdentityDisc);
  }

  if ([self isRunningTest:@selector(testNewTabShowsMIAEntryPointInline)]) {
    config.features_enabled_and_params.push_back(
        {kNTPMIAEntrypoint,
         {{{kNTPMIAEntrypointParam,
            kNTPMIAEntrypointParamOmniboxContainedInline}}}});
    config.features_disabled.push_back(omnibox::kAimServerEligibilityEnabled);
  }

  if ([self isRunningTest:@selector(testNewTabShowsMIAEntryPointInOmnibox)]) {
    config.features_enabled_and_params.push_back(
        {kNTPMIAEntrypoint,
         {{{kNTPMIAEntrypointParam,
            kNTPMIAEntrypointParamOmniboxContainedSingleButton}}}});
    config.features_disabled.push_back(omnibox::kAimServerEligibilityEnabled);
  }
  if ([self isRunningTest:@selector
            (testNewTabShowsMIAEntryPointInEnlargedFakebox)]) {
    config.features_enabled_and_params.push_back(
        {kNTPMIAEntrypoint,
         {{{kNTPMIAEntrypointParam,
            kNTPMIAEntrypointParamOmniboxContainedEnlargedFakebox}}}});
    config.features_disabled.push_back(omnibox::kAimServerEligibilityEnabled);
  }
  if ([self
          isRunningTest:@selector(testIncognitoButtonNotShownInQuickActions)]) {
    config.features_enabled_and_params.push_back(
        {kNTPMIAEntrypoint,
         {{{kNTPMIAEntrypointParam,
            kNTPMIAEntrypointParamEnlargedFakeboxNoIncognito}}}});
    config.features_disabled.push_back(omnibox::kAimServerEligibilityEnabled);
  }

  return config;
}

- (void)tearDownHelper {
  [self releaseHistogramTester];
  policy_test_utils::ClearPolicies();
  [super tearDownHelper];
}

- (void)setupHistogramTester {
  if (self.histogramTesterSet) {
    return;
  }
  self.histogramTesterSet = YES;
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
}

- (void)releaseHistogramTester {
  if (!self.histogramTesterSet) {
    return;
  }
  self.histogramTesterSet = NO;
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);
}

#pragma mark - Helpers

// Sets up the NTP Location policy dynamically at runtime.
- (void)setNTPPolicyValue:(std::string)ntpLocation {
  policy_test_utils::SetPolicyWithStringValue(ntpLocation,
                                              policy::key::kNewTabPageLocation);
}

// Validates that the new tab URL is the expected one.
- (void)validateNTPURL:(GURL)expectedURL {
  // Wait until the page has finished loading.
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Validate the URL.
  const GURL currentURL = [ChromeEarlGrey webStateVisibleURL];
  GREYAssertEqual(expectedURL, currentURL, @"Page navigated unexpectedly to %s",
                  currentURL.spec().c_str());
}

#pragma mark - Tests

// Tests that all items are accessible on the most visited page.
- (void)testAccessibilityOnMostVisited {
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests the metrics are reported correctly.
- (void)testNTPMetrics {
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  [ChromeEarlGrey closeAllTabs];

  // Open and close an NTP.
  [self setupHistogramTester];
  NSError* error =
      [MetricsAppInterface expectTotalCount:0
                               forHistogram:@"NewTabPage.TimeSpent"];
  chrome_test_util::GREYAssertErrorNil(error);
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey closeAllTabs];
  error = [MetricsAppInterface expectTotalCount:1
                                   forHistogram:@"NewTabPage.TimeSpent"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface expectTotalCount:1
                                   forHistogram:@"IOS.NTP.Impression"];
  chrome_test_util::GREYAssertErrorNil(error);
  [self releaseHistogramTester];

  // Open an incognito NTP and close it.
  [ChromeEarlGrey closeAllTabs];
  [self setupHistogramTester];
  error = [MetricsAppInterface expectTotalCount:0
                                   forHistogram:@"NewTabPage.TimeSpent"];
  chrome_test_util::GREYAssertErrorNil(error);
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey closeAllTabs];
  error = [MetricsAppInterface expectTotalCount:0
                                   forHistogram:@"NewTabPage.TimeSpent"];
  error = [MetricsAppInterface expectTotalCount:0
                                   forHistogram:@"IOS.NTP.Impression"];
  chrome_test_util::GREYAssertErrorNil(error);
  chrome_test_util::GREYAssertErrorNil(error);
  [self releaseHistogramTester];

  // Open an NTP and navigate to another URL.
  [self setupHistogramTester];
  error = [MetricsAppInterface expectTotalCount:0
                                   forHistogram:@"NewTabPage.TimeSpent"];
  chrome_test_util::GREYAssertErrorNil(error);
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  error = [MetricsAppInterface expectTotalCount:1
                                   forHistogram:@"NewTabPage.TimeSpent"];
  error = [MetricsAppInterface expectTotalCount:1
                                   forHistogram:@"IOS.NTP.Impression"];
  chrome_test_util::GREYAssertErrorNil(error);
  chrome_test_util::GREYAssertErrorNil(error);
  [self releaseHistogramTester];

  // Open an NTP and switch tab.
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  [self setupHistogramTester];
  error = [MetricsAppInterface expectTotalCount:0
                                   forHistogram:@"NewTabPage.TimeSpent"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface expectTotalCount:0
                                   forHistogram:@"IOS.NTP.Impression"];
  chrome_test_util::GREYAssertErrorNil(error);
  [ChromeEarlGrey openNewTab];
  error = [MetricsAppInterface expectTotalCount:1
                                   forHistogram:@"IOS.NTP.Impression"];
  chrome_test_util::GREYAssertErrorNil(error);
  [ChromeEarlGrey selectTabAtIndex:0];
  error = [MetricsAppInterface expectTotalCount:1
                                   forHistogram:@"NewTabPage.TimeSpent"];
  chrome_test_util::GREYAssertErrorNil(error);
  [ChromeEarlGrey selectTabAtIndex:1];
  error = [MetricsAppInterface expectTotalCount:1
                                   forHistogram:@"NewTabPage.TimeSpent"];
  chrome_test_util::GREYAssertErrorNil(error);
  [ChromeEarlGrey selectTabAtIndex:0];
  error = [MetricsAppInterface expectTotalCount:2
                                   forHistogram:@"NewTabPage.TimeSpent"];
  chrome_test_util::GREYAssertErrorNil(error);
  [self releaseHistogramTester];

  // Open two NTPs and close them.
  [ChromeEarlGrey closeAllTabs];
  [self setupHistogramTester];

  error = [MetricsAppInterface expectTotalCount:0
                                   forHistogram:@"NewTabPage.TimeSpent"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface expectTotalCount:0
                                   forHistogram:@"IOS.NTP.Impression"];
  chrome_test_util::GREYAssertErrorNil(error);
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  error = [MetricsAppInterface expectTotalCount:1
                                   forHistogram:@"NewTabPage.TimeSpent"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface expectTotalCount:2
                                   forHistogram:@"IOS.NTP.Impression"];
  chrome_test_util::GREYAssertErrorNil(error);
  [ChromeEarlGrey closeAllTabs];
  error = [MetricsAppInterface expectTotalCount:2
                                   forHistogram:@"NewTabPage.TimeSpent"];
  chrome_test_util::GREYAssertErrorNil(error);
  error = [MetricsAppInterface expectTotalCount:2
                                   forHistogram:@"IOS.NTP.Impression"];
  [self releaseHistogramTester];
}

// Tests that all items are accessible on the incognito page.
- (void)testAccessibilityOnIncognitoTab {
  [ChromeEarlGrey openNewIncognitoTab];
  GREYAssert(WaitForHistoryToDisappear(), @"History did not disappear.");
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [ChromeEarlGrey closeAllIncognitoTabs];
}

#pragma mark - Policy NTP Location Tests

// Tests that the new tab opens the policy's New Tab Page Location when the URL
// is valid.
- (void)testValidNTPLocation {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL(kPageURL);

  // Set the policy's NTP Location value at runtime.
  [self setNTPPolicyValue:expectedURL.spec()];

  // Open a new tab page.
  [ChromeEarlGrey openNewTab];
  [self validateNTPURL:expectedURL];
}

// Tests that the new tab doesn't open the policy's New Tab Page Location when
// the URL is empty.
- (void)testEmptyNTPLocation {
  // Set the policy's NTP Location value at runtime.
  [self setNTPPolicyValue:""];

  // Open a new tab page.
  [ChromeEarlGrey openNewTab];

  // Verify that the new tab URL is chrome://newtab/.
  const GURL expectedURL(kChromeUINewTabURL);
  [self validateNTPURL:expectedURL];
}

// Tests that the incognito new tab doesn't open the policy's New Tab Page
// Location even if the URL is valid.
- (void)testIncognitoNTPLocation {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL testURL = self.testServer->GetURL(kPageURL);

  // Set the policy's NTP Location value at runtime.
  [self setNTPPolicyValue:testURL.spec()];

  // Open a new incognito tab page.
  [ChromeEarlGrey openNewIncognitoTab];

  // Verify that the new tab URL is chrome://newtab/.
  const GURL expectedURL(kChromeUINewTabURL);
  [self validateNTPURL:expectedURL];

  [ChromeEarlGrey closeAllIncognitoTabs];
}

// Verifies that the app launches with a new tab page with the correct policy's
// New Tab Page Location URL.
- (void)testNewTabOnLaunchWithNTPLocation {
  // Close all existing tabs.
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey closeAllIncognitoTabs];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL(kPageURL);

  // Adds the NTP Location policy to the app's launch configuration.
  AppLaunchConfiguration config;
  config.additional_args.push_back("-NTPLocation");
  config.additional_args.push_back(expectedURL.spec());
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self validateNTPURL:expectedURL];
}

// Verifies opening a new tab from the New Tab button on the toolbar with the
// correct policy's New Tab Page Location URL.
- (void)testNewTabByNewTabButtonTapWithNTPLocation {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL(kPageURL);

  // Set the policy's NTP Location value at runtime.
  [self setNTPPolicyValue:expectedURL.spec()];

  // Open tab via the UI.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NewTabButton()]
      performAction:grey_tap()];

  [self validateNTPURL:expectedURL];
}

// Verifies opening a new tab from the tools menu with the correct policy's New
// Tab Page Location URL.
- (void)testNewTabFromToolsMenuWithNTPLocation {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL(kPageURL);

  // Set the policy's NTP Location value at runtime.
  [self setNTPPolicyValue:expectedURL.spec()];

  // Open tab via the UI.
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> newTabButtonMatcher =
      grey_accessibilityID(kToolsMenuNewTabId);
  [[EarlGrey selectElementWithMatcher:newTabButtonMatcher]
      performAction:grey_tap()];

  [self validateNTPURL:expectedURL];
}

// Verifies opening a new tab by long pressing the tab grid view and selecting
// "New Tab" with the correct policy's New Tab Page Location URL.
- (void)testNewTabByLongPressTabGridViewWithNTPLocation {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL(kPageURL);

  // Set the policy's NTP Location value at runtime.
  [self setNTPPolicyValue:expectedURL.spec()];

  // Open tab via the UI.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_longPress()];

  id<GREYMatcher> menuNewTabButtonMatcher;
  menuNewTabButtonMatcher =
      grey_allOf(chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_TOOLS_MENU_NEW_TAB),
                 grey_ancestor(grey_kindOfClassName(@"UICollectionView")), nil);
  [[EarlGrey selectElementWithMatcher:menuNewTabButtonMatcher]
      performAction:grey_tap()];

  [self validateNTPURL:expectedURL];
}

// Verifies opening a new tab from the tab grid view by tapping on the New Tab
// button with the correct policy's New Tab Page Location URL.
- (void)testNewTabFromTabGridViewWithNTPLocation {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL(kPageURL);

  // Set the policy's NTP Location value at runtime.
  [self setNTPPolicyValue:expectedURL.spec()];

  // Open tab via the UI.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridNewTabButton()]
      performAction:grey_tap()];

  [self validateNTPURL:expectedURL];
}

// Tests that the error badge is shown on top of the identity disc when the
// primary account has a persistent error.
// TODO(crbug.com/394268777): Reenable test.
- (void)DISABLED_testErrorBadge {
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Wait for the error badge to appear.
  id<GREYMatcher> errorBadgeMatcher =
      grey_accessibilityID(kNTPFeedHeaderIdentityDiscBadge);
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:errorBadgeMatcher]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return !error;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Error badge didn't appear in the allotted time");

  // Tap on the identity disc to open Settings.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kNTPFeedHeaderIdentityDisc)]
      performAction:grey_tap()];

  // Open account settings.
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsAccountButton()];

  // Verify the error section is showing.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncErrorButtonIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap "Enter Passphrase" button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncErrorButtonIdentifier)]
      performAction:grey_tap()];

  // Enter the passphrase.
  [SigninEarlGreyUI submitSyncPassphrase:kPassphrase];

  // Dismiss settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];

  // Verify the error badge on the ADP disappears.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kNTPFeedHeaderIdentityDiscBadge)]
      assertWithMatcher:grey_notVisible()];
}

#pragma mark - MIA Variations

// Verifies the MIA entry point visiblity for the inline variation.
- (void)testNewTabShowsMIAEntryPointInline {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad.");
  }
  // Open a new tab page.
  [ChromeEarlGrey openNewTab];
  // Verify the MIA button is shown.
  VerifyMIAButtonVisible(true);
  // Quick actions should not be visible when MIA is displayed inline.
  VerifyQuickActionVisibility(QuickActionsVisibility::kNotVisible);
}

// Verifies the MIA entry point visiblity for the omnibox contained variation.
- (void)testNewTabShowsMIAEntryPointInOmnibox {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad.");
  }
  // Open a new tab page.
  [ChromeEarlGrey openNewTab];
  // Verify the MIA button is shown.
  VerifyMIAButtonVisible(true);
  // Quick actions should not be visible.
  VerifyQuickActionVisibility(QuickActionsVisibility::kVisible);
}

// Verifies the MIA entry point visiblity for the enlarged fakebox variation.
- (void)testNewTabShowsMIAEntryPointInEnlargedFakebox {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad.");
  }

  // Open a new tab page.
  [ChromeEarlGrey openNewTab];
  // Verify the MIA button is shown.
  VerifyMIAButtonVisible(true);
  // Quick actions should be visible.
  VerifyQuickActionVisibility(QuickActionsVisibility::kVisible);
}

// Verifies that the quick actions menu doesn't show incognito for one specific
// MIA entry point variation.
- (void)testIncognitoButtonNotShownInQuickActions {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad.");
  }

  // Open a new tab page.
  [ChromeEarlGrey openNewTab];
  // Verify the MIA button is shown.
  VerifyMIAButtonVisible(true);
  // QUick actions should not be visible.
  VerifyQuickActionVisibility(QuickActionsVisibility::kVisibleWithoutIncognito);
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/policy/policy_constants.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/supervised_user/core/browser/supervised_user_url_filter.h"
#import "components/supervised_user/core/common/features.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/supervised_user_settings_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {
static const char* kHost = "a.host";
static const char* kEchoPath = "/echo";
static const char* kEchoContent = "Echo";
static const char* kDefaultPath = "/defaultresponse";
static const char* kDefaultContent = "Default response";
static const char* kInterstitialContent = "Ask your parent";
static const char* kInterstitialWaitingContent = "Waiting for permission";
static const char* kInterstitialBlockReason = "This site is blocked";
static const char* kInterstitialDetails = "Details";
static const char* kInterstitialFirstTimeBanner =
    "Family Link choices for Chrome apply here";
}  // namespace

// Tests the core user journeys of a supervised user with FamilyLink parental
// control restrictions enabled.
@interface SupervisedUserWithParentalControlsTestCase : ChromeTestCase
@end

@implementation SupervisedUserWithParentalControlsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kIOSQuickDelete);
  return config;
}

- (void)signInSupervisedUser {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
}

- (void)setUp {
  [super setUp];
  bool started = self.testServer->Start();
  GREYAssertTrue(started, @"Test server failed to start.");
  [SupervisedUserSettingsAppInterface setUpTestUrlLoaderFactoryHelper];
}

- (void)tearDown {
  [ChromeEarlGrey closeCurrentTab];
  [SupervisedUserSettingsAppInterface resetSupervisedUserURLFilterBehavior];
  [SupervisedUserSettingsAppInterface resetManualUrlFiltering];
  [SupervisedUserSettingsAppInterface tearDownTestUrlLoaderFactoryHelper];
  [super tearDown];
}

- (void)checkRequestSentMessageVisibility:(BOOL)isVisible {
  NSString* isRequestSentMessageVisible =
      [NSString stringWithFormat:
                    @"%sdocument.getElementById('request-sent-message').hidden",
                    isVisible ? "!" : ""];
  [ChromeEarlGrey waitForJavaScriptCondition:isRequestSentMessageVisible];
}

- (void)checkBlockPageHeaderVisibility:(BOOL)isVisible {
  NSString* isBlockPageHeaderVisible = [NSString
      stringWithFormat:@"%sdocument.getElementById('block-page-header').hidden",
                       isVisible ? "!" : ""];
  [ChromeEarlGrey waitForJavaScriptCondition:isBlockPageHeaderVisible];
}

- (void)checkShowDetailsLinkVisibility:(BOOL)isVisible {
  [self checkElementDisplayStyleVisibility:@"block-reason-show-details-link"
                                 isVisible:isVisible];
}

- (void)checkHideDetailsLinkVisibility:(BOOL)isVisible {
  [self checkElementDisplayStyleVisibility:@"block-reason-hide-details-link"
                                 isVisible:isVisible];
}

- (void)checkElementDisplayStyleVisibility:(NSString*)elementId
                                 isVisible:(BOOL)isVisible {
  NSString* isElementVisible =
      [NSString stringWithFormat:@"getComputedStyle(document.getElementById('%@"
                                 @"')).display %s== \"none\"",
                                 elementId, isVisible ? "!" : "="];
  [ChromeEarlGrey waitForJavaScriptCondition:isElementVisible];
}

- (void)checkInterstitalIsShown {
  [ChromeEarlGrey waitForWebStateContainingText:kInterstitialContent];
  // Originally the "Ask your parent" title is shown and the "Waiting
  // for pemission" message is hidden.
  [self checkBlockPageHeaderVisibility:YES];
  [self checkRequestSentMessageVisibility:NO];
}

- (void)checkInterstitalIsShownInWaitingScreen {
  [ChromeEarlGrey waitForWebStateContainingText:kInterstitialWaitingContent];
  // In waiting screen, the "Ask your parent" title is hidden and the "Waiting
  // for pemission" message is visible instead.
  [self checkBlockPageHeaderVisibility:NO];
  [self checkRequestSentMessageVisibility:YES];
}

- (void)clearBrowsingData {
  // Disable closing tabs as it's on by default in delete browsing data, so the
  // tab closure animation is not run in iPads.
  [ChromeEarlGrey setBoolValue:false
                   forUserPref:browsing_data::prefs::kCloseTabs];

  // Clear the browsing data.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI
      tapPrivacyMenuButton:chrome_test_util::ClearBrowsingDataCell()];

  // "Browsing history", "Cookies, Site Data" and "Cached Images and Files"
  // are the default checked options when the prefs are registered. No need to
  // modify them.
  [ChromeEarlGreyUI tapClearBrowsingDataMenuButton:
                        chrome_test_util::ClearBrowsingDataButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testSupervisedUserSignin DISABLED_testSupervisedUserSignin
#else
#define MAYBE_testSupervisedUserSignin testSupervisedUserSignin
#endif
// TODO(crbug.com/331644931): Re-enable on device when fixed.
// Tests that the user is signed in.
- (void)MAYBE_testSupervisedUserSignin {
  [self signInSupervisedUser];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that the supervised user does not see popular content suggestions.
- (void)testSupervisedUserOpenNewTabPage {
  [self signInSupervisedUser];

  [ChromeEarlGreyUI openNewTab];

  // Assert that the most visited tiles are not visible for supervised users.
  id<GREYMatcher> firstMostVisitedTile = grey_accessibilityID(
      [kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix
          stringByAppendingString:@"0"]);
  [[EarlGrey selectElementWithMatcher:firstMostVisitedTile]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testSupervisedUserURLFilteringReloadsOnlyRealizedExistingWebStates \
  DISABLED_testSupervisedUserURLFilteringReloadsOnlyRealizedExistingWebStates
#else
#define MAYBE_testSupervisedUserURLFilteringReloadsOnlyRealizedExistingWebStates \
  testSupervisedUserURLFilteringReloadsOnlyRealizedExistingWebStates
#endif
// TODO(crbug.com/331644931): Re-enable on device when fixed.
// Tests that only realized existing web states will display the interstitial
// when a filtering for them is triggered. Also tests that the filtering logic
// on existing tabs does not force-realize unrealized states. This is a
// regression test for bug: 1486459.
- (void)
    MAYBE_testSupervisedUserURLFilteringReloadsOnlyRealizedExistingWebStates {
  // Signing in the user and allow all sites.
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowAllSites];

  // Open three tabs, visit a webpage from them and check they are unblocked.
  GURL URL = self.testServer->GetURL(kEchoPath);
  for (int i = 0; i < 3; i++) {
    if (i != 0) {
      [ChromeEarlGreyUI openNewTab];
    }
    [ChromeEarlGrey loadURL:URL];
    [ChromeEarlGrey waitForWebStateContainingText:kEchoContent];
  }

  // Restart the browser (needed to obtain non-realized states).
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  // Add the switch to make sure that the user stays signed in in the restart.
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kSignInAtStartup);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Check the previously used tabs are maintained.
  [ChromeEarlGrey waitForMainTabCount:3];
  // Set up histogram tracking before changing the filtering behaviour.
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");
  // Change the filtering setting to block the previously used urls. This
  // results in a new filtering of the existing tabs.
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  // There should be one realized web state (active tab).
  // Check that only one tab displays the intersitial.
  GREYAssertEqual(
      1, [ChromeEarlGrey realizedWebStatesCount],
      @"A single realized web state must exist. The tab reloading filtering"
      @"behaviour should not force web states to become realized.");

  // Wait for one interstitial to appear (on the realized tab).
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForPageLoadTimeout,
          ^bool {
            return
                [SupervisedUserSettingsAppInterface
                    countSupervisedUserIntersitialsForExistingWebStates] == 1;
          }),
      @"Interstitial did not appear.");

  // Out of the 3 tabs, only the active one should have recorded metrics for
  // filtering.
  auto* error =
      [MetricsAppInterface expectTotalCount:1
                               forHistogram:@"ManagedUsers.FilteringResult"];
  if (error) {
    GREYFail([error description]);
  }
}

#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testSupervisedUserSignedOutOnPolicyChange \
  DISABLED_testSupervisedUserSignedOutOnPolicyChange
#else
#define MAYBE_testSupervisedUserSignedOutOnPolicyChange \
  testSupervisedUserSignedOutOnPolicyChange
#endif
// TODO(crbug.com/331644931): Re-enable on device when fixed.
// Tests that the user is correctly signed out after signin is disabled via
// policy.
- (void)MAYBE_testSupervisedUserSignedOutOnPolicyChange {
  [self signInSupervisedUser];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Disable signin via policy.
  policy_test_utils::SetPolicy(static_cast<int>(BrowserSigninMode::kDisabled),
                               policy::key::kBrowserSignin);

  // Make sure the UI has settled and the last changes are taken into account.
  GREYWaitForAppToIdle(@"App failed to idle");

  NSString* continueLabel =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT_CONTINUE);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(continueLabel),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];

  [SigninEarlGrey verifySignedOut];
  [PolicyAppInterface clearPolicies];
}

// Tests that the Encryption item is disabled for supervised users.
- (void)testEncryptionItemDisabledForSupervisedUsers {
  [self signInSupervisedUser];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsAccountButton()];

  [[[EarlGrey selectElementWithMatcher:
                  grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                                 IDS_IOS_MANAGE_SYNC_ENCRYPTION),
                             grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];
}

#pragma mark - Filtering Behaviour

// Tests that users with "Allow Approved" filtering are shown the interstitial
// when they navigate to a non-approved site.
- (void)testSupervisedUserWithAllowApprovedSitesFilteringIsShownInterstitial {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  GURL blockedURL = self.testServer->GetURL(kEchoPath);
  [ChromeEarlGrey loadURL:blockedURL];

  [self checkInterstitalIsShown];
}

// Tests that users with "Allow All" filtering are shown the interstitial
// when they navigate to a site that ClassifyUrl classifies as unsafe.
- (void)testSupervisedUserWithAllowAllSitesAndSafeSearchRestricted {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowAllSites];
  [SupervisedUserSettingsAppInterface
      setDefaultClassifyURLNavigationIsAllowed:NO];

  // When safe search classifies the url as restricted, the user navigation is
  // blocked.
  GURL blockedSafeSearchURL = self.testServer->GetURL(kEchoPath);
  [ChromeEarlGrey loadURL:blockedSafeSearchURL];

  [self checkInterstitalIsShown];
}

// Tests that users with "Allow All" filtering are not blocked
// when they navigate to a website allowed by ClassifyUrl.
- (void)testSupervisedUserWithAllowAllSitesAndSafeSearchAllowed {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowAllSites];
  // TODO(b/297313665): Instead of a default response, introduce a stack-based
  // approach for the mocked reponses. See `kids_management_api_server_mock.h`.
  [SupervisedUserSettingsAppInterface
      setDefaultClassifyURLNavigationIsAllowed:YES];

  // When safe search classifies the url as allowed, the user can navigate to
  // it.
  GURL allowedSafeSearchURL = self.testServer->GetURL(kEchoPath);
  [ChromeEarlGrey loadURL:allowedSafeSearchURL];

  [ChromeEarlGrey waitForWebStateContainingText:kEchoContent];
}

// Tests that users with "Allow All" filtering are shown the interstitial
// when they navigate to a site from the blocked list.
- (void)
    testSupervisedUserWithAllowAllSitesFilteringIsShownInterstitialOnBlockedSite {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowAllSites];

  GURL blockedURL = self.testServer->GetURL(kHost, kEchoPath);
  [SupervisedUserSettingsAppInterface
      addWebsiteToBlockList:net::NSURLWithGURL(blockedURL)];

  [ChromeEarlGrey loadURL:blockedURL];
  [self checkInterstitalIsShown];
}

// Tests that users with "Allow Approved" filtering are not blocked
// when they navigate to an allow-listed website.
- (void)testSupervisedUserWithAllowApprovedSitesFilteringCanViewAllowedWebages {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  GURL URL = self.testServer->GetURL(kEchoPath);
  // The page is originally blocked.
  [ChromeEarlGrey loadURL:URL];
  [self checkInterstitalIsShown];

  // Navigate to another page to change the browser content (otherwise,
  // allow-listing the site will trigger an immediate refresh - not the scope
  // of this test, see
  // `testSupervisedUserWithAllowAllFilteringIsBlockedOnUrlBlockListing`-).
  GURL otherURL = self.testServer->GetURL(kHost, kEchoPath);
  [ChromeEarlGrey loadURL:otherURL];
  [self checkInterstitalIsShown];

  // Allow-list the page and re-visit it. It should now be unblocked.
  [SupervisedUserSettingsAppInterface
      addWebsiteToAllowList:net::NSURLWithGURL(URL)];

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:kEchoContent];
}

// Tests that when an interstitial is displayed for a blocked site,
// allow-listing it triggers an intestitial refresh and unblocks the page.
- (void)
    testSupervisedUserWithAllowApprovedFilteringIsUnblockedOnURLAllowListing {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  GURL URL = self.testServer->GetURL(kEchoPath);
  [ChromeEarlGrey loadURL:URL];
  [self checkInterstitalIsShown];

  [SupervisedUserSettingsAppInterface
      addWebsiteToAllowList:net::NSURLWithGURL(URL)];
  // Ensure that the interstitial is refreshed and the un-blocked page is
  // displayed.
  [ChromeEarlGrey waitForWebStateContainingText:kEchoContent];
}

// Tests that block-listing a url, results in showing immediately the
// interstitial if the user has the url open in a tab.
- (void)testSupervisedUserWithAllowAllFilteringIsBlockedOnURLBlockListing {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowAllSites];

  GURL URL = self.testServer->GetURL(kEchoPath);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:kEchoContent];

  [SupervisedUserSettingsAppInterface
      addWebsiteToBlockList:net::NSURLWithGURL(URL)];
  // Ensure that the interstitial is triggered.
  [self checkInterstitalIsShown];
}

// Tests that users who have the filtering behaviour changed from "Allow all"
// to "Allow approved" websites, will be shown the interstitial as soon as
// the filtering behaviour changes.
- (void)
    testSupervisedUserWithAllowApprovedSitesFilteringIsBlockedOnFilterChange {
  [self signInSupervisedUser];
  GURL safeURL = self.testServer->GetURL(kEchoPath);
  [ChromeEarlGrey loadURL:safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:kEchoContent];

  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];
  [self checkInterstitalIsShown];

  // Reloading the page should not affect the interstitial.
  [ChromeEarlGrey reload];
  [self checkInterstitalIsShown];
}

// Tests that for users who have the filtering behaviour changed from "Allow
// approved" to "Allow all" websites, a blocked pages will be refreshed and
// unblocks as soon as the filtering behaviour changes.
- (void)testSupervisedUserWithAllowAllSitesFilteringIsUnblockedOnFilterChange {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  GURL blockedURL = self.testServer->GetURL(kEchoPath);
  [ChromeEarlGrey loadURL:blockedURL];
  [self checkInterstitalIsShown];

  [SupervisedUserSettingsAppInterface setFilteringToAllowAllSites];
  // Ensure that the interstitial is refreshed and the un-blocked page is
  // displayed.
  [ChromeEarlGrey waitForWebStateContainingText:kEchoContent];
}

// Tests that users who navigate to a blocked page can request that it is
// unblocked and upon unblocking the page is refreshed and displayed.
- (void)testSupervisedUserWithAllowAllSitesFilteringCanUnblockRequestedWebsite {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFakePermissionCreator];
  [SupervisedUserSettingsAppInterface setFilteringToAllowAllSites];

  GURL blockedURL = self.testServer->GetURL(kEchoPath);
  [SupervisedUserSettingsAppInterface
      addWebsiteToBlockList:net::NSURLWithGURL(blockedURL)];

  [ChromeEarlGrey loadURL:blockedURL];
  [self checkInterstitalIsShown];

  // On clicking "Ask in a message" button, the interstitial "Waiting" screen is
  // displayed.
  [ChromeEarlGrey tapWebStateElementWithID:@"remote-approvals-button"];
  [self checkInterstitalIsShownInWaitingScreen];

  // Approving the permission request for the blocked host
  // should refresh the newly unblocked page.
  [SupervisedUserSettingsAppInterface
      approveWebsiteDomain:net::NSURLWithGURL(blockedURL)];
  [ChromeEarlGrey waitForWebStateContainingText:kEchoContent];
}

#pragma mark - Interstitial UI Behaviour

// Checks the behaviour of the "Details" link on click (expand/shrink details).
- (void)testSupervisedUserShowInterstitialDetailsLinkOnClickForNarrowScreen {
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"This is an iphone test case only.");
#endif
  // Compact width only.
  if (![ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_DISABLED(@"This is a narrow screen test case only.");
  }

  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFakePermissionCreator];
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  GURL blockedURL = self.testServer->GetURL(kHost, kEchoPath);
  [ChromeEarlGrey loadURL:blockedURL];
  [self checkInterstitalIsShown];
  [ChromeEarlGrey waitForWebStateContainingText:kInterstitialDetails];
  [self checkShowDetailsLinkVisibility:YES];
  [self checkHideDetailsLinkVisibility:NO];

  // Expand the Details link.
  [ChromeEarlGrey tapWebStateElementWithID:@"block-reason-show-details-link"];
  [ChromeEarlGrey waitForWebStateContainingText:"This site is blocked"];
  [self checkShowDetailsLinkVisibility:NO];
  [self checkHideDetailsLinkVisibility:YES];

  // Shrink the Details link.
  [ChromeEarlGrey tapWebStateElementWithID:@"block-reason-hide-details-link"];
  [ChromeEarlGrey waitForWebStateContainingText:kInterstitialDetails];
  [self checkShowDetailsLinkVisibility:YES];
  [self checkHideDetailsLinkVisibility:NO];
}

// Checks that we don't regress to b/290000817: The 'Details' link should
// be absernt from the interstitial 'Waiting' screen bor both existing (updated)
// intersitials and new interstitials for already requested hosts.
- (void)testSupervisedUserShowInterstitialDetailsLinkForNarrowScreen {
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"This is an iphone test case only.");
#endif
  // Compact width only.
  if (![ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_DISABLED(@"This is a narrow screen test case only.");
  }

  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFakePermissionCreator];
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  GURL blockedURL = self.testServer->GetURL(kHost, kEchoPath);
  [ChromeEarlGrey loadURL:blockedURL];
  [self checkInterstitalIsShown];

  // Details link must be visible.
  [ChromeEarlGrey waitForWebStateContainingText:kInterstitialDetails];
  [self checkShowDetailsLinkVisibility:YES];
  [self checkHideDetailsLinkVisibility:NO];

  // Case 1: Requested host on present (updated) intersitial:
  // The Details link must not be visible on the
  // "Waiting" screen on the existing interstitial.
  [ChromeEarlGrey tapWebStateElementWithID:@"remote-approvals-button"];
  [self checkInterstitalIsShownInWaitingScreen];
  [self checkShowDetailsLinkVisibility:NO];
  [self checkHideDetailsLinkVisibility:NO];

  // Case 2: Already requested host on a new intersitial case:
  // Tge Details link must not be visible on the
  // "Waiting" screen on the new interstitial.
  GURL otherURL = self.testServer->GetURL("other.host", kEchoPath);
  [ChromeEarlGrey loadURL:otherURL];
  [self checkInterstitalIsShown];

  // Request the original blocked site. The interstitial "Waiting" screen is
  // displayed without the Details.
  [ChromeEarlGrey loadURL:blockedURL];
  [self checkInterstitalIsShownInWaitingScreen];
  [self checkShowDetailsLinkVisibility:NO];
  [self checkHideDetailsLinkVisibility:NO];
}

// Tests that the that the Details link / Block reason is displayed on the
// interstitial "Ask your parent" screen depending on the screen width.
- (void)testSupervisedUserInterstitialShowBlockReasonAndDetails {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  GURL blockedURL = self.testServer->GetURL(kHost, kEchoPath);

  [ChromeEarlGrey loadURL:blockedURL];
  [self checkInterstitalIsShown];

  if ([ChromeEarlGrey isCompactWidth]) {
    // Narrow screen displays "Details" link ("Block reason" is hidden).
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialDetails];
    [self checkElementDisplayStyleVisibility:@"block-reason-show-details-link"
                                   isVisible:YES];
    [self checkElementDisplayStyleVisibility:@"block-reason" isVisible:NO];
  } else {
    // Wide screen displays "Block reason" ("Details" is hidden).
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialBlockReason];
    [self checkElementDisplayStyleVisibility:@"block-reason-show-details-link"
                                   isVisible:NO];
    [self checkElementDisplayStyleVisibility:@"block-reason" isVisible:YES];
  }
}

// Tests that the Back Button of the interstitial gets us to the previous page.
- (void)testSupervisedUserInterstitialOnBackButton {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFakePermissionCreator];
  [SupervisedUserSettingsAppInterface setFilteringToAllowAllSites];

  GURL allowedURL = self.testServer->GetURL(kDefaultPath);
  GURL blockedURL = self.testServer->GetURL(kHost, kEchoPath);
  [SupervisedUserSettingsAppInterface
      addWebsiteToBlockList:net::NSURLWithGURL(blockedURL)];

  [ChromeEarlGrey loadURL:allowedURL];
  [ChromeEarlGrey waitForWebStateContainingText:kDefaultContent];

  [ChromeEarlGrey loadURL:blockedURL];
  [self checkInterstitalIsShown];

  // On clicking "Ask in a message" button, the interstitial "Waiting" screen is
  // displayed.
  [ChromeEarlGrey tapWebStateElementWithID:@"remote-approvals-button"];
  [self checkInterstitalIsShownInWaitingScreen];

  // On clicking the "Ok" (back) button, the browser goes to the previous
  // (allowed) page.
  [ChromeEarlGrey tapWebStateElementWithID:@"back-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kDefaultContent];
}

// Tests that for already requested for approval urls, the interstitial is shown
// in the waiting screen upon revisiting.
- (void)testSupervisedUserInterstitialForAlreadyRequestedHostShowsWaitScreen {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFakePermissionCreator];
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  GURL blockedURL = self.testServer->GetURL(kHost, kEchoPath);

  [ChromeEarlGrey loadURL:blockedURL];
  [self checkInterstitalIsShown];

  // On clicking "Ask in a message" button, the interstitial "Waiting" screen is
  // displayed.
  [ChromeEarlGrey tapWebStateElementWithID:@"remote-approvals-button"];
  [self checkInterstitalIsShownInWaitingScreen];

  // Navigate to another site.
  GURL otherURL = self.testServer->GetURL("other.host", kEchoPath);
  [ChromeEarlGrey loadURL:otherURL];
  [self checkInterstitalIsShown];

  // Navigate to the original blocked site. The interstitial "Waiting" screen is
  // displayed.
  [ChromeEarlGrey loadURL:blockedURL];
  [self checkInterstitalIsShownInWaitingScreen];
}

// Tests that users are shown the First Time Banner on the interstitial on their
// first navigation to a blocked page.
- (void)testSupervisedUserInterstitialDisplaysFirstTimeBanner {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface resetFirstTimeBanner];
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  // On the first blocked site the interstitial displays the first time banner.
  GURL blockedURL = self.testServer->GetURL(kEchoPath);
  [ChromeEarlGrey loadURL:blockedURL];
  [self checkInterstitalIsShown];
  [ChromeEarlGrey waitForWebStateContainingText:kInterstitialFirstTimeBanner];
  [self checkElementDisplayStyleVisibility:@"banner" isVisible:YES];

  // Navigate to another blocked site. The banner should not be visible anymore.
  blockedURL = self.testServer->GetURL("other.host", kEchoPath);
  [ChromeEarlGrey loadURL:blockedURL];
  [self checkInterstitalIsShown];
  [self checkElementDisplayStyleVisibility:@"banner" isVisible:NO];
}

// Tests that the Zoom Text option is available for the interstitial.
- (void)testSupervisedUserInterstitialSupportsZoom {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  GURL blockedURL = self.testServer->GetURL(kEchoPath);
  [ChromeEarlGrey loadURL:blockedURL];
  [self checkInterstitalIsShown];

  // Verify the Zoom Text button is available and clickable.
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kToolsMenuTextZoom),
                                   grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_not(grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled))];
}

#pragma mark - Clear Content Behaviour

// Tests that a user in the legacy "syncing" state remains signed in after
// clearing the browsing data (Cookies and BrowsingHistory).
// TODO(crbug.com/40066949): Delete this test after the syncing state is gone.
- (void)testSupervisedUserWithLegacySyncStaysSignedInAfterClearingBrowsingData {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];
  [SigninEarlGrey signinAndEnableLegacySyncFeature:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [self clearBrowsingData];

  // The user should be still signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that a signed in user remains signed in after clearing the browsing
// data (Cookies and BrowsingHistory).
- (void)testSupervisedUserStaysSignedInAfterClearingBrowsingData {
  [self signInSupervisedUser];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [self clearBrowsingData];

  // The user should be still signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

@end

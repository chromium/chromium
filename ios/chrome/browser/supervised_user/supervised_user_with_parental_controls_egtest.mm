// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/supervised_user/core/browser/supervised_user_url_filter.h"
#import "components/supervised_user/core/common/features.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/supervised_user_settings_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/base/mac/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
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

// TODO(b/296996910): Add test cases for the option "Blocked Mature
// Content".

@implementation SupervisedUserWithParentalControlsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
  return config;
}

- (void)signInSupervisedUserWithSync:(BOOL)withSync {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey setIsSubjectToParentalControls:YES forIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:withSync];
}

- (void)signInSupervisedUser {
  [self signInSupervisedUserWithSync:YES];
}

- (void)setUp {
  [super setUp];
  bool started = self.testServer->Start();
  GREYAssertTrue(started, @"Test server failed to start.");
}

- (void)tearDown {
  [ChromeEarlGrey closeCurrentTab];
  [SupervisedUserSettingsAppInterface resetSupervisedUserURLFilterBehavior];
  [SupervisedUserSettingsAppInterface resetManualUrlFiltering];
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
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          ConfirmClearBrowsingDataButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the user is signed in.
- (void)testSupervisedUserSignin {
  [self signInSupervisedUser];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
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

// Tests that users with "Allow All" filtering are not blocked
// when they navigate to a site (allowed by default).
- (void)testSupervisedUserWithAllowAllSitesFilteringCanViewWebages {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowAllSites];

  GURL allowedURL = self.testServer->GetURL(kEchoPath);
  [ChromeEarlGrey loadURL:allowedURL];
  [ChromeEarlGrey waitForWebStateContainingText:kEchoContent];
}

// Tests that users with "Allow All" filtering are shown the interstitial
// when they navigate to a site from the blocked list.
- (void)
    testSupervisedUserWithAllowAllSitesFilteringIsShownInterstitialOnBlockedSite {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowAllSites];

  GURL blockedUrl = self.testServer->GetURL(kHost, kEchoPath);
  [SupervisedUserSettingsAppInterface
      addWebsiteToBlockList:net::NSURLWithGURL(blockedUrl)];

  [ChromeEarlGrey loadURL:blockedUrl];
  [self checkInterstitalIsShown];
}

// Tests that users with "Allow Approved" filtering are not blocked
// when they navigate to an allow-listed website.
- (void)testSupervisedUserWithAllowApprovedSitesFilteringCanViewAllowedWebages {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  GURL url = self.testServer->GetURL(kEchoPath);
  // The page is originally blocked.
  [ChromeEarlGrey loadURL:url];
  [self checkInterstitalIsShown];

  // Navigate to another page to change the browser content (otherwise,
  // allow-listing the site will trigger an immediate refresh - not the scope
  // of this test, see
  // `testSupervisedUserWithAllowAllFilteringIsBlockedOnUrlBlockListing`-).
  GURL otherUrl = self.testServer->GetURL(kHost, kEchoPath);
  [ChromeEarlGrey loadURL:otherUrl];
  [self checkInterstitalIsShown];

  // Allow-list the page and re-visit it. It should now be unblocked.
  [SupervisedUserSettingsAppInterface
      addWebsiteToAllowList:net::NSURLWithGURL(url)];

  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:kEchoContent];
}

// Tests that when an interstitial is displayed for a blocked site,
// allow-listing it triggers an intestitial refresh and unblocks the page.
- (void)
    testSupervisedUserWithAllowApprovedFilteringIsUnblockedOnUrlAllowListing {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  GURL url = self.testServer->GetURL(kEchoPath);
  [ChromeEarlGrey loadURL:url];
  [self checkInterstitalIsShown];

  [SupervisedUserSettingsAppInterface
      addWebsiteToAllowList:net::NSURLWithGURL(url)];
  // Ensure that the interstitial is refreshed and the un-blocked page is
  // displayed.
  [ChromeEarlGrey waitForWebStateContainingText:kEchoContent];
}

// Tests that block-listing a url, results in showing immediately the
// interstitial if the user has the url open in a tab.
- (void)testSupervisedUserWithAllowAllFilteringIsBlockedOnUrlBlockListing {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowAllSites];

  GURL url = self.testServer->GetURL(kEchoPath);
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:kEchoContent];

  [SupervisedUserSettingsAppInterface
      addWebsiteToBlockList:net::NSURLWithGURL(url)];
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

  GURL blockedUrl = self.testServer->GetURL(kEchoPath);
  [SupervisedUserSettingsAppInterface
      addWebsiteToBlockList:net::NSURLWithGURL(blockedUrl)];

  [ChromeEarlGrey loadURL:blockedUrl];
  [self checkInterstitalIsShown];

  // On clicking "Ask in a message" button, the interstitial "Waiting" screen is
  // displayed.
  [ChromeEarlGrey tapWebStateElementWithID:@"remote-approvals-button"];
  [self checkInterstitalIsShownInWaitingScreen];

  // Approving the permission request for the blocked host
  // should refresh the newly unblocked page.
  [SupervisedUserSettingsAppInterface
      approveWebsiteDomain:net::NSURLWithGURL(blockedUrl)];
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

  GURL blockedUrl = self.testServer->GetURL(kHost, kEchoPath);
  [ChromeEarlGrey loadURL:blockedUrl];
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

  GURL blockedUrl = self.testServer->GetURL(kHost, kEchoPath);
  [ChromeEarlGrey loadURL:blockedUrl];
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
  GURL otherUrl = self.testServer->GetURL("other.host", kEchoPath);
  [ChromeEarlGrey loadURL:otherUrl];
  [self checkInterstitalIsShown];

  // Request the original blocked site. The interstitial "Waiting" screen is
  // displayed without the Details.
  [ChromeEarlGrey loadURL:blockedUrl];
  [self checkInterstitalIsShownInWaitingScreen];
  [self checkShowDetailsLinkVisibility:NO];
  [self checkHideDetailsLinkVisibility:NO];
}

// Tests that the that the Details link / Block reason is displayed on the
// interstitial "Ask your parent" screen depending on the screen width.
- (void)testSupervisedUserInterstitialShowBlockReasonAndDetails {
  [self signInSupervisedUser];
  [SupervisedUserSettingsAppInterface setFilteringToAllowApprovedSites];

  GURL blockedUrl = self.testServer->GetURL(kHost, kEchoPath);

  [ChromeEarlGrey loadURL:blockedUrl];
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

  GURL allowedUrl = self.testServer->GetURL(kDefaultPath);
  GURL blockedUrl = self.testServer->GetURL(kHost, kEchoPath);
  [SupervisedUserSettingsAppInterface
      addWebsiteToBlockList:net::NSURLWithGURL(blockedUrl)];

  [ChromeEarlGrey loadURL:allowedUrl];
  [ChromeEarlGrey waitForWebStateContainingText:kDefaultContent];

  [ChromeEarlGrey loadURL:blockedUrl];
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

  GURL blockedUrl = self.testServer->GetURL(kHost, kEchoPath);

  [ChromeEarlGrey loadURL:blockedUrl];
  [self checkInterstitalIsShown];

  // On clicking "Ask in a message" button, the interstitial "Waiting" screen is
  // displayed.
  [ChromeEarlGrey tapWebStateElementWithID:@"remote-approvals-button"];
  [self checkInterstitalIsShownInWaitingScreen];

  // Navigate to another site.
  GURL otherUrl = self.testServer->GetURL("other.host", kEchoPath);
  [ChromeEarlGrey loadURL:otherUrl];
  [self checkInterstitalIsShown];

  // Navigate to the original blocked site. The interstitial "Waiting" screen is
  // displayed.
  [ChromeEarlGrey loadURL:blockedUrl];
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

// Tests that a logged in user with enabled "Sync" remains logged in after
// clearing the browsing data (Cookies and BrowsingHistory).
- (void)testSupervisedUserWithSyncIsLoggedInAfterClearingBrowsingData {
  [self signInSupervisedUserWithSync:YES];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [self clearBrowsingData];

  // The user should be still logged in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that a logged in user with disabled "Sync" remains logged in after
// clearing the browsing data (Cookies and BrowsingHistory).
- (void)testSupervisedUserWithoutSyncIsLoggedInAfterClearingBrowsingData {
  [self signInSupervisedUserWithSync:NO];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [self clearBrowsingData];

  // The user should be still logged in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

@end

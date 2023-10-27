// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <map>
#import <string>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_prefs.h"
#import "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/test_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/history/history_ui_constants.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_app_interface.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/tabs/tests/distant_tabs_app_interface.h"
#import "ios/chrome/browser/ui/tabs/tests/fake_distant_tab.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::PrimarySignInButton;
using chrome_test_util::RecentTabsDestinationButton;

namespace {

const char kURLOfTestPage[] = "http://testPage";
const char kHTMLOfTestPage[] =
    "<head><title>TestPageTitle</title></head><body>hello</body>";
NSString* const kTitleOfTestPage = @"TestPageTitle";

// Timeout in seconds to wait for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

// Sign in and sync using a fake identity.
void SignInAndSync() {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fake_identity];
  [SigninEarlGreyUI signinWithFakeIdentity:fake_identity enableSync:YES];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
}

// Sign out and clear sync data.
void SignOut() {
  [SigninEarlGrey signOut];
  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey clearSyncServerData];
}

// Makes sure at least one tab is opened and opens the recent tab panel.
void OpenRecentTabsPanel() {
  // At least one tab is needed to be able to open the recent tabs panel.
  if ([ChromeEarlGrey isIncognitoMode]) {
    if ([ChromeEarlGrey incognitoTabCount] == 0)
      [ChromeEarlGrey openNewIncognitoTab];
  } else {
    if ([ChromeEarlGrey mainTabCount] == 0)
      [ChromeEarlGrey openNewTab];
  }

  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:RecentTabsDestinationButton()];
}

// Returns the matcher for the Recent Tabs table.
id<GREYMatcher> RecentTabsTable() {
  return grey_accessibilityID(
      kRecentTabsTableViewControllerAccessibilityIdentifier);
}

// Returns the matcher for the entry of the page in the recent tabs panel.
id<GREYMatcher> TitleOfTestPage() {
  return grey_allOf(
      grey_ancestor(RecentTabsTable()),
      chrome_test_util::StaticTextWithAccessibilityLabel(kTitleOfTestPage),
      grey_sufficientlyVisible(), nil);
}

GURL TestPageURL() {
  return web::test::HttpServer::MakeUrl(kURLOfTestPage);
}

}  // namespace

// Earl grey integration tests for Recent Tabs Panel Controller.
@interface RecentTabsTestCase : WebHttpServerChromeTestCase
@end

@implementation RecentTabsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isRunningTest:@selector(testSyncTypesListDisabled)]) {
    // Configure the policy.
    config.additional_args.push_back(
        "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
    config.additional_args.push_back(
        "<dict><key>SyncTypesListDisabled</key><array><string>tabs</"
        "string></array></dict>");
  }
  if ([self isRunningTest:@selector
            (testShowPromoIfSignedOut_SyncToSigninDisabled)] ||
      [self isRunningTest:@selector
            (testShowPromoIfSignedIn_SyncToSigninDisabled)]) {
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
  if ([self isRunningTest:@selector
            (testShowPromoIfSignedOutAndHasAccounts_SyncToSigninEnabled)] ||
      [self isRunningTest:@selector
            (testShowPromoIfSignedOutAndNoAccounts_SyncToSigninEnabled)] ||
      [self isRunningTest:@selector
            (testDelineHistorySyncIfSignedOut_SyncToSigninEnabled)] ||
      [self isRunningTest:@selector
            (testDelineRepeatedlyHistorySyncIfSignedIn_SyncToSigninEnabled)] ||
      [self isRunningTest:@selector
            (testShowPromoIfSignedInAndTabsDisabled_SyncToSigninEnabled)] ||
      [self isRunningTest:@selector
            (testDelineHistorySyncIfSignedInAndTabsDisabled_SyncToSigninEnabled
                )] ||
      [self isRunningTest:@selector
            (testNoPromoIfSignedInAndTabsEnabled_SyncToSigninEnabled)]) {
    config.features_enabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];
  web::test::SetUpSimpleHttpServer(std::map<GURL, std::string>{{
      TestPageURL(),
      std::string(kHTMLOfTestPage),
  }});
  [RecentTabsAppInterface clearCollapsedListViewSectionStates];
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  [PolicyAppInterface clearPolicies];
}

// Tests reopening a closed tab from a regular tab.
- (void)testOpenCloseTabFromRegular {
  const GURL testPageURL = TestPageURL();

  // Open the test page in a new tab.
  [ChromeEarlGrey loadURL:testPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello"];

  // Open the Recent Tabs panel, check that the test page is not
  // present.
  OpenRecentTabsPanel();
  [[EarlGrey selectElementWithMatcher:TitleOfTestPage()]
      assertWithMatcher:grey_nil()];
  [self closeRecentTabs];

  // Close the tab containing the test page and wait for the stack view to
  // appear.
  [ChromeEarlGrey closeCurrentTab];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Open the Recent Tabs panel and check that the test page is present.
  OpenRecentTabsPanel();
  [[EarlGrey selectElementWithMatcher:TitleOfTestPage()]
      assertWithMatcher:grey_notNil()];

  // Tap on the entry for the test page in the Recent Tabs panel and check that
  // a tab containing the test page was opened.
  [[EarlGrey selectElementWithMatcher:TitleOfTestPage()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(
                            testPageURL.GetContent())];

  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that tapping "Show Full History" open the history.
- (void)testOpenHistory {
  OpenRecentTabsPanel();

  // Tap "Show Full History"
  id<GREYMatcher> showHistoryMatcher =
      grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_HISTORY_SHOWFULLHISTORY_LINK),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:showHistoryMatcher]
      performAction:grey_tap()];

  // Make sure history is opened.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              l10n_util::GetNSString(
                                                  IDS_HISTORY_TITLE)),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitHeader),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Close History.
  id<GREYMatcher> exitMatcher =
      grey_accessibilityID(kHistoryNavigationControllerDoneButtonIdentifier);
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];

  // Close tab.
  [ChromeEarlGrey closeCurrentTab];
}

// Tests that a promo to sign in + sync is shown to a signed out user.
// kReplaceSyncPromosWithSignInPromos is disabled.
- (void)testShowPromoIfSignedOut_SyncToSigninDisabled {
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount
                           closeButton:NO];

  // Accept the promo.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kConfirmationAccessibilityIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that a promo to sign in is shown to a signed out user without device
// accounts. Tapping the promo shows the auth activity then the history opt-in.
// kReplaceSyncPromosWithSignInPromos is enabled.
- (void)testShowPromoIfSignedOutAndNoAccounts_SyncToSigninEnabled {
  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  // Sign in with fake identity.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts
                           closeButton:NO];

  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      performAction:grey_tap()];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentityForSSOAuthAddAccountFlow:fakeIdentity];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kFakeAuthAddAccountButtonIdentifier),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that a promo to sign in is shown to a signed out user who has device
// accounts. Tapping the promo shows the sign-in sheet then the history opt-in.
// kReplaceSyncPromosWithSignInPromos is enabled.
- (void)testShowPromoIfSignedOutAndHasAccounts_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Open recent tabs.
  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount
                           closeButton:NO];
  // Tap on promo "Turn on" button.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      performAction:grey_tap()];
  // Confirm sign in.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Verify that the footer is shown without the user's email.
  NSString* disclaimerText =
      l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_FOOTER_WITHOUT_EMAIL);
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(disclaimerText),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_notNil()];
  // Accept History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];
  // Verify that the history sync is enabled.
  GREYAssertTrue(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be enabled.");
  GREYAssertTrue([SigninEarlGreyAppInterface
                     isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                 @"Tabs sync should be enabled.");
  // TODO(crbug.com/1467853): Verify that sync consent is granted.
  // Verify that MSBB consent is granted.
  GREYAssertTrue(
      [ChromeEarlGrey
          userBooleanPref:unified_consent::prefs::
                              kUrlKeyedAnonymizedDataCollectionEnabled],
      @"MSBB consent was not granted.");
  // Verify that the identity is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that for a signed out user, sign-in using the promo then decline
// history sync promo signs the user out and does not enable history sync.
// kReplaceSyncPromosWithSignInPromos is enabled.
- (void)testDelineHistorySyncIfSignedOut_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Open Recent Tabs.
  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  // Tap on promo "Turn on" button.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      performAction:grey_tap()];
  // Confirm sign in.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Decline History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];
  // Verify that the history sync is disabled.
  GREYAssertFalse(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be disabled.");
  GREYAssertFalse([SigninEarlGreyAppInterface
                      isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                  @"Tabs sync should be disabled.");
  // TODO(crbug.com/1467853): Verify that sync consent is not granted.
  // Verify that MSBB consent is not granted.
  GREYAssertFalse(
      [ChromeEarlGrey
          userBooleanPref:unified_consent::prefs::
                              kUrlKeyedAnonymizedDataCollectionEnabled],
      @"MSBB consent should not be granted.");
  // Verify that the identity is signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests that for a signed in user, after declining twice History Sync, the
// History Sync is still shown when tapping on the promo action button.
- (void)testDelineRepeatedlyHistorySyncIfSignedIn_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  // Open Recent Tabs.
  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap on promo button and decline History Sync 3 times.
  for (int i = 0; i <= 2; i++) {
    // Tap on "Turn on" button.
    [[EarlGrey
        selectElementWithMatcher:
            grey_allOf(grey_accessibilityID(
                           kRecentTabsTabSyncOffButtonAccessibilityIdentifier),
                       grey_accessibilityTrait(UIAccessibilityTraitButton),
                       grey_sufficientlyVisible(), nil)]
        performAction:grey_tap()];
    // Verify that the History Sync Opt-In screen is shown.
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(
                                     kHistorySyncViewAccessibilityIdentifier)]
        assertWithMatcher:grey_sufficientlyVisible()];
    // Decline History Sync.
    [[[EarlGrey selectElementWithMatcher:
                    chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()]
           usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
        onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
        performAction:grey_tap()];
    [ChromeEarlGrey
        waitForUIElementToDisappearWithMatcher:
            grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];
  }

  // Verify that the History Sync is disabled.
  GREYAssertFalse(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be disabled.");
  GREYAssertFalse([SigninEarlGreyAppInterface
                      isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                  @"Tabs sync should be disabled.");
}

// Tests that no promo to sign-in + sync is shown to a user who is signed out
// but has sign-in disabled by policy.
- (void)testNoPromoIfSignedOutAndSigninDisabledByPolicy {
  policy_test_utils::SetPolicy(static_cast<int>(BrowserSigninMode::kDisabled),
                               policy::key::kBrowserSignin);

  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that no promo to sign-in + sync is shown to a signed-out user if sync
// is disabled by policy.
// Note this also applies when kReplaceSyncPromosWithSignInPromos is enabled:
// even though kSyncDisabled doesn't block sign-in, there's no sense in
// promoting sign-in if the user won't be able to see their tabs from other
// devices.
- (void)testNoPromoIfSignedOutAndSyncDisabledByPolicy {
  // Set the policy and dismiss the bottom sheet that it causes.
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_SYNC_SYNC_DISABLED_CONTINUE))]
      performAction:grey_tap()];

  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that a promo to sync is shown to a signed-in non-syncing user.
// kReplaceSyncPromosWithSignInPromos is disabled.
- (void)testShowPromoIfSignedIn_SyncToSigninDisabled {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];

  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSyncWithPrimaryAccount
                           closeButton:NO];

  // Accept the promo.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kConfirmationAccessibilityIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that the tab sync promo is shown to a signed-in user who hasn't
// opted in yet.
// kReplaceSyncPromosWithSignInPromos is enabled.
- (void)testShowPromoIfSignedInAndTabsDisabled_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  // Open Recent Tabs.
  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  // Tap on "Turn on" button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kRecentTabsTabSyncOffButtonAccessibilityIdentifier),
                     grey_accessibilityTrait(UIAccessibilityTraitButton),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Verify that the footer is shown with the user's email.
  NSString* disclaimerText =
      l10n_util::GetNSStringF(IDS_IOS_HISTORY_SYNC_FOOTER_WITH_EMAIL,
                              base::SysNSStringToUTF16(fakeIdentity.userEmail));
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(disclaimerText),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_notNil()];
  // Accept History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];
  // Verify that the history sync is enabled.
  GREYAssertTrue(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be enabled.");
  GREYAssertTrue([SigninEarlGreyAppInterface
                     isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                 @"Tabs sync should be enabled.");
  // TODO(crbug.com/1467853): Verify that sync consent is granted.
  // Verify that MSBB consent is granted.
  GREYAssertTrue(
      [ChromeEarlGrey
          userBooleanPref:unified_consent::prefs::
                              kUrlKeyedAnonymizedDataCollectionEnabled],
      @"MSBB consent was not granted.");
  // Verify that the identity is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that for a signed-in user, declining history sync does not sign the
// user out and does not enable history sync.
// kReplaceSyncPromosWithSignInPromos is enabled.
- (void)testDelineHistorySyncIfSignedInAndTabsDisabled_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  // Open Recent Tabs
  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  // Tap on "Turn on" button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kRecentTabsTabSyncOffButtonAccessibilityIdentifier),
                     grey_accessibilityTrait(UIAccessibilityTraitButton),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Decline History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];
  // Verify that the history sync is disabled.
  GREYAssertFalse(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be disabled.");
  GREYAssertFalse([SigninEarlGreyAppInterface
                      isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                  @"Tabs sync should be disabled.");
  // TODO(crbug.com/1467853): Verify that sync consent is not granted.
  // Verify that MSBB consent is not granted.
  GREYAssertFalse(
      [ChromeEarlGrey
          userBooleanPref:unified_consent::prefs::
                              kUrlKeyedAnonymizedDataCollectionEnabled],
      @"MSBB consent should not be granted.");
  // Verify that the identity is still signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests no promo is shown to a signed-in user who has already opted in to
// tab sync.
// kReplaceSyncPromosWithSignInPromos is enabled.
- (void)testNoPromoIfSignedInAndTabsEnabled_SyncToSigninEnabled {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  [SigninEarlGreyAppInterface
      setSelectedType:(syncer::UserSelectableType::kTabs)
              enabled:YES];

  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI verifySigninPromoNotVisible];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kRecentTabsTabSyncOffButtonAccessibilityIdentifier),
                     grey_accessibilityTrait(UIAccessibilityTraitButton), nil)]
      assertWithMatcher:grey_nil()];
}

// Tests no promo to sync is shown to a signed-in non-syncing user if sync is
// disabled by policy.
// TODO(crbug.com/1487984): Test fails on official builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define MAYBE_testNoPromoIfSignedInAndSyncDisabledByPolicy \
  DISABLED_testNoPromoIfSignedInAndSyncDisabledByPolicy
#else
#define MAYBE_testNoPromoIfSignedInAndSyncDisabledByPolicy \
  testNoPromoIfSignedInAndSyncDisabledByPolicy
#endif
- (void)MAYBE_testNoPromoIfSignedInAndSyncDisabledByPolicy {
  // Set the policy and dismiss the bottom sheet that it causes.
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_SYNC_SYNC_DISABLED_CONTINUE))]
      performAction:grey_tap()];

  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];

  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests no promo is shown to a syncing user with tab sync enabled.
- (void)testNoPromoIfSyncing {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:YES];

  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests a promo is shown to a syncing user who disabled the tab sync toggle.
// Tapping the promo opens the page to re-enable the toggle.
- (void)testShowPromoIfSyncingAndDisabledTabs {
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:YES];
  [SigninEarlGreyAppInterface
      setSelectedType:(syncer::UserSelectableType::kTabs)
              enabled:NO];

  OpenRecentTabsPanel();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  id<GREYMatcher> promoButtonMatcher = grey_allOf(
      grey_accessibilityID(kRecentTabsTabSyncOffButtonAccessibilityIdentifier),
      grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
  [[EarlGrey selectElementWithMatcher:promoButtonMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:promoButtonMatcher]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   [ChromeEarlGrey
                                       isReplaceSyncWithSigninEnabled]
                                       ? IDS_IOS_HISTORY_SYNC_TITLE
                                       : IDS_IOS_MANAGE_SYNC_SETTINGS_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the sign-in promo can be reloaded correctly.
- (void)testRecentTabSigninPromoReloaded {
  OpenRecentTabsPanel();

  // Scroll to sign-in promo, if applicable.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Sign-in promo should be visible with no accounts on the device.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts
                           closeButton:NO];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Sign-in promo should be visible with an account on the device.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount
                           closeButton:NO];
  [self closeRecentTabs];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
}

// Tests that the sign-in promo can be reloaded correctly while being hidden.
// crbug.com/776939
- (void)testRecentTabSigninPromoReloadedWhileHidden {
  OpenRecentTabsPanel();

  // Scroll to sign-in promo, if applicable
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts
                           closeButton:NO];

  // Tap on "Other Devices", to hide the sign-in promo.
  NSString* otherDevicesLabel =
      l10n_util::GetNSString(IDS_IOS_RECENT_TABS_OTHER_DEVICES);
  id<GREYMatcher> otherDevicesMatcher = grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabel(otherDevicesLabel),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:otherDevicesMatcher]
      performAction:grey_tap()];
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  // Add an account.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Tap on "Other Devices", to show the sign-in promo.
  [[EarlGrey selectElementWithMatcher:otherDevicesMatcher]
      performAction:grey_tap()];
  // Scroll to sign-in promo, if applicable
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount
                           closeButton:NO];
  [self closeRecentTabs];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
}

// Tests that the VC can be dismissed by swiping down.
- (void)testSwipeDownDismiss {
  // TODO(crbug.com/1129589): Test disabled on iOS14 iPhones.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iOS14 iPhones.");
  }
  OpenRecentTabsPanel();

  id<GREYMatcher> recentTabsViewController =
      grey_allOf(RecentTabsTable(), grey_sufficientlyVisible(), nil);

  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:recentTabsViewController]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:recentTabsViewController]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:recentTabsViewController]
      assertWithMatcher:grey_nil()];

  [ChromeEarlGrey closeCurrentTab];
}

// Tests that the Recent Tabs can be opened while signed in (prevent regression
// for https://crbug.com/1056613).
- (void)testOpenWhileSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  OpenRecentTabsPanel();
}

// Tests that there is a text cell in the Recently Closed section when it's
// empty.
- (void)testRecentlyClosedEmptyState {
  OpenRecentTabsPanel();

  id<GREYInteraction> detailTextCell = [EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabelId(
                         IDS_IOS_RECENT_TABS_RECENTLY_CLOSED_EMPTY),
                     grey_sufficientlyVisible(), nil)];
  [detailTextCell assertWithMatcher:grey_notNil()];
}

// Tests that the Signin promo is visible in the Other Devices section and that
// the illustrated cell is present.
- (void)testOtherDevicesDefaultEmptyState {
  OpenRecentTabsPanel();

  id<GREYInteraction> illustratedCell = [EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(
                  kRecentTabsOtherDevicesIllustratedCellAccessibilityIdentifier),
              grey_sufficientlyVisible(), nil)];
  [illustratedCell assertWithMatcher:grey_notNil()];

  // Scroll to sign-in promo, if applicable
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts
                           closeButton:NO];
}

// Tests that the distant session is correctly displayed and tapping on a
// distant tab correctly open it.
- (void)testOtherDevicesWithOneDistantSession {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  SignInAndSync();

  NSString* sessionName = @"Desktop";
  NSUInteger numberOfTabs = 4;

  // Create a distant session with 4 tabs.
  [DistantTabsAppInterface
      addSessionToFakeSyncServer:sessionName
               modifiedTimeDelta:base::Minutes(5)
                            tabs:[FakeDistantTab
                                     createFakeTabsForServerURL:self.testServer
                                                                    ->base_url()
                                                   numberOfTabs:numberOfTabs]];
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SESSIONS];

  OpenRecentTabsPanel();

  // The illustrated cell should not be visible.
  id<GREYInteraction> illustratedCell = [EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(
                  kRecentTabsOtherDevicesIllustratedCellAccessibilityIdentifier),
              grey_sufficientlyVisible(), nil)];
  [illustratedCell assertWithMatcher:grey_nil()];

  // Check that the distant session is displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(sessionName)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that all distant tabs are displayed.
  for (NSUInteger i = 0; i < numberOfTabs; ++i) {
    // Check that the session header is displayed.
    NSString* tabName = [NSString stringWithFormat:@"Tab %ld", i];
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(tabName),
                                            grey_ancestor(grey_kindOfClassName(
                                                @"TableViewURLCell")),
                                            nil)]
        assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Open a distant tab and check that the location bar shows the distant tab
  // URL in a short form.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Tab 0")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:chrome_test_util::LocationViewContainingText(
                            self.testServer->base_url().host())];

  SignOut();
}

// Tests that all the distant sessions are correctly displayed.
- (void)testOtherDevicesWithMultipleDistantSessions {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  SignInAndSync();

  NSArray<NSString*>* sessionNames =
      @[ @"Desktop", @"Phone", @"Tablet", @"iPad", @"iPhone", @"MacBook" ];
  NSUInteger numberOfTabs = 4;

  // Create distant sessions.
  for (NSUInteger i = 0; i < sessionNames.count; ++i) {
    NSString* sessionName = sessionNames[i];
    [DistantTabsAppInterface
        addSessionToFakeSyncServer:sessionName
                 modifiedTimeDelta:base::Minutes(5)
                              tabs:
                                  [FakeDistantTab
                                      createFakeTabsForServerURL:
                                          self.testServer->base_url()
                                                    numberOfTabs:numberOfTabs]];
  }
  [ChromeEarlGrey triggerSyncCycleForType:syncer::SESSIONS];

  OpenRecentTabsPanel();

  // The illustrated cell should not be visible.
  id<GREYInteraction> illustratedCell = [EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(
                  kRecentTabsOtherDevicesIllustratedCellAccessibilityIdentifier),
              grey_sufficientlyVisible(), nil)];
  [illustratedCell assertWithMatcher:grey_nil()];

  // Check that all distant sessions are displayed.
  for (NSUInteger i = sessionNames.count - 1; i > 0; --i) {
    NSString* sessionName = sessionNames[i];
    // Tap the session header to collapse the section.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(sessionName)]
        performAction:grey_tap()];
  }

  SignOut();
}

// Tests the Copy Link action on a recent tab's context menu.
- (void)testContextMenuCopyLink {
  [self loadTestURL];
  OpenRecentTabsPanel();
  [self longPressTestURLTab];

  GURL testURL = TestPageURL();
  [ChromeEarlGrey
      verifyCopyLinkActionWithText:[NSString
                                       stringWithUTF8String:testURL.spec()
                                                                .c_str()]];
}

// Tests the Open in New Window action on a recent tab's context menu.
// TODO(crbug.com/1273942) Test is flaky.
- (void)FLAKY_testContextMenuOpenInNewWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  [self loadTestURL];
  OpenRecentTabsPanel();

  [self longPressTestURLTab];

  [ChromeEarlGrey verifyOpenInNewWindowActionWithContent:"hello"];

  // Validate that Recent tabs was not closed in the original window. The
  // Accessibility Element matcher is added as there are other (non-accessible)
  // recent tabs tables in each window's TabGrid (but hidden).
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(RecentTabsTable(),
                                          grey_accessibilityElement(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests the Share action on a recent tab's context menu.
- (void)testContextMenuShare {
  [self loadTestURL];
  OpenRecentTabsPanel();
  [self longPressTestURLTab];

  const GURL testPageURL = web::test::HttpServer::MakeUrl(kURLOfTestPage);
  [ChromeEarlGrey verifyShareActionWithURL:testPageURL
                                 pageTitle:kTitleOfTestPage];
}

#pragma mark Helper Methods

// Opens a new tab and closes it, to make sure it appears as a recently closed
// tab.
- (void)loadTestURL {
  const GURL testPageURL = web::test::HttpServer::MakeUrl(kURLOfTestPage);

  // Open the test page in a new tab.
  [ChromeEarlGrey loadURL:testPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello"];

  // Close the tab, making it appear in Recent Tabs.
  [ChromeEarlGrey closeCurrentTab];
}

// Long-presses on a recent tab entry.
- (void)longPressTestURLTab {
  // The test page may be there multiple times.
  [[[EarlGrey selectElementWithMatcher:TitleOfTestPage()] atIndex:0]
      performAction:grey_longPress()];
}

// Closes the recent tabs panel.
- (void)closeRecentTabs {
  id<GREYMatcher> exitMatcher =
      grey_accessibilityID(kTableViewNavigationDismissButtonId);
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];
  // Wait until the recent tabs panel is dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Tests that the sign-in promo isn't shown and the 'Other Devices' section is
// managed when the SyncDisabled policy is enabled.
- (void)testSyncDisabled {
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);

  // Dismiss the popup.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_SYNC_SYNC_DISABLED_CONTINUE)),
                            grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];

  OpenRecentTabsPanel();

  // Check that the sign-in promo is not visible.
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  // Check that the 'Other Devices' section is managed.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_RECENT_TABS_DISABLED_BY_ORGANIZATION)),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the sign-in promo isn't shown and the 'Other Devices' section is
// managed when the SyncTypesListDisabled tabs item policy is selected.
- (void)testSyncTypesListDisabled {
  OpenRecentTabsPanel();

  // Check that the sign-in promo is not visible.
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  // Check that the 'Other Devices' section is managed.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_RECENT_TABS_DISABLED_BY_ORGANIZATION)),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the sign-in promo isn't shown and the 'Other Devices' section has
// the managed notice footer when sign-in is disabled by the BrowserSignin
// policy.
- (void)testSignInDisabledByPolicy {
  policy_test_utils::SetPolicy(static_cast<int>(BrowserSigninMode::kDisabled),
                               policy::key::kBrowserSignin);

  OpenRecentTabsPanel();

  // Check that the sign-in promo is not visible.
  [SigninEarlGreyUI verifySigninPromoNotVisible];

  // Check that the 'Other Devices' section has the managed notice.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_RECENT_TABS_DISABLED_BY_ORGANIZATION)),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

@end

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "build/branding_buildflags.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/signin/ios/browser/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::kWaitForPageLoadTimeout;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::GoogleSyncSettingsButton;
using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsSignInRowMatcher;
using chrome_test_util::SignOutAccountsButton;

namespace {

// Returns a matcher for the sign-in screen "Continue as <identity>" button.
id<GREYMatcher> GetContinueButtonWithIdentityMatcher(
    FakeSystemIdentity* fakeIdentity) {
  NSString* buttonTitle = l10n_util::GetNSStringF(
      IDS_IOS_FIRST_RUN_SIGNIN_CONTINUE_AS,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));
  id<GREYMatcher> matcher =
      grey_allOf(grey_accessibilityLabel(buttonTitle),
                 grey_accessibilityTrait(UIAccessibilityTraitStaticText),
                 grey_sufficientlyVisible(), nil);

  return matcher;
}

// Returns a matcher for the whole forced sign-in screen.
id<GREYMatcher> GetForcedSigninScreenMatcher() {
  return grey_accessibilityID(
      first_run::kFirstRunSignInScreenAccessibilityIdentifier);
}

// Checks that the forced sign-in prompt is fully dismissed by making sure
// that there isn't any forced sign-in screen displayed.
void VerifyForcedSigninFullyDismissed() {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kTangibleSyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Scrolls down to `elementMatcher` in the scrollable content of the first run
// screen.
void ScrollToElementAndAssertVisibility(id<GREYMatcher> elementMatcher) {
  id<GREYMatcher> scrollView =
      grey_accessibilityID(kPromoStyleScrollViewAccessibilityIdentifier);

  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(elementMatcher,
                                          grey_kindOfClassName(@"UILabel"),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:scrollView] assertWithMatcher:grey_notNil()];
}

// Signs in the browser from the forced sign-in screen.
void WaitForForcedSigninScreenAndSignin(FakeSystemIdentity* fakeIdentity) {
  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];

  // Scroll to the "Continue as ..." button to go to the bottom of the screen.
  ScrollToElementAndAssertVisibility(
      GetContinueButtonWithIdentityMatcher(fakeIdentity));

  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      performAction:grey_tap()];
}

// Opens account settings and signs out from them.
void OpenAccountSettingsAndSignOut() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  // We're now in the "manage sync" view, and the signout button is at the very
  // bottom. Scroll there.
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:scrollViewMatcher]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Sign out" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM))]
      performAction:grey_tap()];

  // Check that the sign-out snackbar does not show for BrowserSignin forced.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityLabel(l10n_util::GetNSString(
              IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_SNACKBAR_MESSAGE))]
      assertWithMatcher:grey_notVisible()];
}

// Sets up the sign-in policy value dynamically at runtime.
void SetSigninEnterprisePolicyValue(BrowserSigninMode signinMode) {
  policy_test_utils::SetPolicy(static_cast<int>(signinMode),
                               policy::key::kBrowserSignin);
}

// Simulates opening `URL` from another application.
void SimulateExternalAppURLOpeningWithURL(NSURL* URL) {
  [ChromeEarlGrey simulateExternalAppURLOpeningWithURL:URL];
  GREYWaitForAppToIdle(@"App failed to idle");
}

// Waits until the loading of the page opened at `openedURL` is done.
void WaitUntilPageLoadedWithURL(NSURL* openedURL) {
  GURL openedGURL = net::GURLWithNSURL(openedURL);
  GREYCondition* startedLoadingCondition = [GREYCondition
      conditionWithName:@"Page has started loading"
                  block:^{
                    return openedGURL == [ChromeEarlGrey webStateVisibleURL];
                  }];
  BOOL pageStartedLoading = [startedLoadingCondition
      waitWithTimeout:kWaitForPageLoadTimeout.InSecondsF()];
  GREYAssertTrue(pageStartedLoading, @"Page did not start loading");
  // Wait until the page has finished loading.
  [ChromeEarlGrey waitForPageToFinishLoading];
  GREYWaitForAppToIdle(@"App failed to idle");
}

constexpr char kPageURL[] = "/test.html";

// Response handler for `kPageURL` that servers a dummy test page.
std::unique_ptr<net::test_server::HttpResponse> PageHttpResponse(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kPageURL) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content("<html><head><title>Hello World</title></head>"
                             "<body>Hello World!</body></html>");
  return std::move(http_response);
}
// Returns grey matcher for an item in Google Service Settings with `titleID`
// and `detailTextID`.
id<GREYMatcher> GetItemMatcherWithTitleAndTextIDs(int titleID,
                                                  int detailTextID) {
  NSString* accessibilityLabel = l10n_util::GetNSString(titleID);
  if (detailTextID) {
    accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", accessibilityLabel,
                                   l10n_util::GetNSString(detailTextID)];
  }
  return grey_allOf(grey_accessibilityLabel(accessibilityLabel),
                    grey_kindOfClassName(@"UITableViewCell"),
                    grey_sufficientlyVisible(), nil);
}

// Opens the Google services settings.
void OpenGoogleServicesSettings() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::GoogleServicesSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kGoogleServicesSettingsViewIdentifier)]
      assertWithMatcher:grey_notNil()];
}

void CompleteSigninFlow() {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          WebSigninPrimaryButtonMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
      performAction:grey_tap()];
}

}  // namespace

// Test the forced sign-in screens.
@interface ForcedSigninTestCase : ChromeTestCase

@end

@implementation ForcedSigninTestCase

- (AppLaunchConfiguration)appConfigurationWithoutEnterprisePolicy {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config =
      [self appConfigurationWithoutEnterprisePolicy];
  // Configure the policy to force sign-in.
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(
      "<dict><key>BrowserSignin</key><integer>2</integer></dict>");

  if ([self isRunningTest:@selector
            (testSignOutFromAccountsOnThisDeviceSyncDisabled)]) {
    // Once kIdentityDiscAccountMenu is launched, the sign out button in
    // ManageAccountsSettings will be removed. It will be safe to remove this
    // test at that point. Note: testSignOutFromAccountMenuForcedSignin
    // covers this policy for the account menu sign-out flow.
    config.features_disabled.push_back(kIdentityDiscAccountMenu);
  } else if ([self isRunningTest:@selector
                   (testSignOutFromAccountMenuForcedSignin)]) {
    config.features_enabled.push_back(kIdentityDiscAccountMenu);
  }

  return config;
}

// Restarts the app without enterprise policies.
- (void)restartAppWithoutEnterprisePolicy {
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:
          [self appConfigurationWithoutEnterprisePolicy]];
}

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
}

- (void)tearDown {
  // Sign out then wait for the sign-in screen to reappear if not already
  // displayed. This is to avoid a conflict between the dismiss animation and
  // the presentation animation of the sign-in screen UI which can be triggered
  // simultaneously when tearing down the test case. The sign-in UI may be
  // triggered again when tearing down because the browser is signed out. Making
  // sure that sign-out is done and that the sign-in screen animation is done
  // before tearing down avoids the conflict.
  [ChromeEarlGrey signOutAndClearIdentities];
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];

  [super tearDown];
}

#pragma mark - Tests

// Tests the sign-in screen with accounts that are already available.
- (void)testSignInScreenWithAccount {
  // Add an identity to sign-in to enable the "Continue as ..." button in the
  // sign-in screen.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Validate the Title text of the forced sign-in screen.
  id<GREYMatcher> title = grey_text(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE_SIGNIN_FORCED));
  ScrollToElementAndAssertVisibility(title);

  // Validate the Subtitle text of the forced sign-in screen.
  id<GREYMatcher> subtitle = grey_text(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SIGNIN_FORCED));
  ScrollToElementAndAssertVisibility(subtitle);

  // Scroll to the "Continue as ..." button to go to the bottom of the screen.
  ScrollToElementAndAssertVisibility(
      GetContinueButtonWithIdentityMatcher(fakeIdentity));

  // Check that there isn't the button to skip sign-in.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_DONT_SIGN_IN))]
      assertWithMatcher:grey_nil()];

  // Touch the continue button to go to the next screen.
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      performAction:grey_tap()];

  VerifyForcedSigninFullyDismissed();
}

// Tests the sign-in screen without accounts where an account has to be added
// before signing in.
- (void)testSignInScreenWithoutAccount {
  // Tap on the "Sign in" button.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION))]
      performAction:grey_tap()];

  // Check for the fake SSO screen.
  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(kFakeAuthActivityViewIdentifier)];
  // Close the SSO view controller.
  id<GREYMatcher> matcher =
      grey_allOf(grey_accessibilityID(kFakeAuthCancelButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  // Make sure the SSO view controller is fully removed before ending the test.
  // The tear down needs to remove other view controllers, and it cannot be done
  // during the animation of the SSO view controler.
  [ChromeEarlGreyUI waitForAppToIdle];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Check that the title of the primary button updates for `fakeIdentity`.
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check the "Sign in" button isn't there anymore.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION))]
      assertWithMatcher:grey_nil()];

  // Check that there isn't the button to skip sign-in.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_DONT_SIGN_IN))]
      assertWithMatcher:grey_nil()];
}

// Tests that accounts can be switched and that there is the button add a new
// account.
- (void)testSignInScreenSwitchAccount {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Tap on the account switcher.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];

  // Check that `fakeIdentity2` is displayed.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that 'Add Account' is displayed.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_ADD_ACCOUNT))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select `fakeIdentity2`.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      performAction:grey_tap()];

  // Check that the title of the primary button updates for `fakeIdentity2`.
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity2)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests signing out account from settings with sync disabled.
- (void)testSignOutFromAccountSettingSyncDisabled {
  // Add account.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Sign in account without enabling sync.
  WaitForForcedSigninScreenAndSignin(fakeIdentity1);

  // Make sure the forced sign-in screen isn't shown.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_nil()];

  // Sign out account from account settings.
  OpenAccountSettingsAndSignOut();

  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests signing out account from accounts on this device with sync disabled.
- (void)testSignOutFromAccountsOnThisDeviceSyncDisabled {
  // Add account.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Sign in account without enabling sync.
  WaitForForcedSigninScreenAndSignin(fakeIdentity1);

  // Make sure the forced sign-in screen isn't shown.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_nil()];

  // Sign out account from accounts on this device settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  // We're now in the "manage sync" view, and the "manage accounts on this
  // device" button is at the very bottom. Scroll there.
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:scrollViewMatcher]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "manage accounts on this device" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_ACCOUNTS_ITEM))]
      performAction:grey_tap()];

  // Tap the "Sign out" button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSettingsAccountsTableViewSignoutCellId)]
      performAction:grey_tap()];

  // Check that the sign-out snackbar does not show for BrowserSignin forced.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityLabel(l10n_util::GetNSString(
              IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_SNACKBAR_MESSAGE))]
      assertWithMatcher:grey_notVisible()];

  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests signing out account from account menu with forced sign-in.
- (void)testSignOutFromAccountMenuForcedSignin {
  // Add account.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Sign in account without enabling sync.
  WaitForForcedSigninScreenAndSignin(fakeIdentity1);

  // Make sure the forced sign-in screen isn't shown.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_nil()];

  // Open the account menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kNTPFeedHeaderIdentityDisc)]
      performAction:grey_tap()];

  // Tap on "Sign Out".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuSignoutButtonId)]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];

  // Check that the sign-out snackbar does not show for BrowserSignin forced.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityLabel(l10n_util::GetNSString(
              IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_SNACKBAR_MESSAGE))]
      assertWithMatcher:grey_notVisible()];

  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests that the sign-out footer has the right text when the user is signed in
// and not syncing with forced sign-in enabled.
- (void)testSignOutFooterForSignInOnlyUserWithForcedSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign in from forced sign-in prompt.
  ScrollToElementAndAssertVisibility(
      GetContinueButtonWithIdentityMatcher(fakeIdentity));
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      performAction:grey_tap()];

  // Open account settings and verify the content of the sign-out footer.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityLabel(l10n_util::GetNSString(
                  IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE_WITH_LEARN_MORE)),
              grey_sufficientlyVisible(), nil)] assertWithMatcher:grey_nil()];
}

// Tests that the forced sign-in prompt can be shown on dynamic policy update
// when a browser modal is displayed on top of the browser view.
- (void)testSignInScreenOnModal {
  // Restart the app to reset the policies.
  [self restartAppWithoutEnterprisePolicy];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Open the settings menu which represents a modal.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGrey waitForMatcher:chrome_test_util::SettingsCollectionView()];

  // Enable the forced sign-in policy to show the forced sign-in prompt.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Wait and verify that the forced sign-in screen is shown when the policy is
  // enabled and the browser is signed out.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests that the forced sign-in prompt can be shown on dynamic policy update
// when on the tab switcher.
- (void)testSignInScreenOnTabSwitcher {
  // Restart the app to reset the policies.
  [self restartAppWithoutEnterprisePolicy];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Close all tabs in the current mode to go on the tab switcher.
  [ChromeEarlGrey closeAllTabsInCurrentMode];

  // Enable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Wait and verify that the forced sign-in screen is shown when the policy is
  // enabled and the browser is signed out.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests that the forced sign-in prompt can be shown on dynamic policy update
// when on an incognito browser tab.
- (void)testSignInScreenOnIncognito {
  // Restart the app to reset the policies.
  [self restartAppWithoutEnterprisePolicy];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Make the surface to present the prompt on an incognito tab.
  [ChromeEarlGrey openNewIncognitoTab];

  // Enable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Wait and verify that the forced sign-in screen is shown when the policy is
  // enabled and the browser is signed out.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests that the forced sign-in prompt is shown after the sign-in prompt when
// sign-in is skipped.
- (void)testSignInScreenDuringRegularSigninPrompt {
  // Restart the app to reset the policies.
  [self restartAppWithoutEnterprisePolicy];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Open the regular sign-in prompt from settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];

  // Enable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Dismiss the regular sign-in prompt by skipping it.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebSigninSkipButtonMatcher()]
      performAction:grey_tap()];

  // Wait and verify that the forced sign-in screen is shown when the policy is
  // enabled and the browser is signed out.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests that the forced sign-in prompt isn't shown when sign-in is done from
// the regular sign-in prompt.
- (void)testNoSignInScreenWhenSigninFromRegularSigninPrompt {
  // Restart the app to reset the policies.
  [self restartAppWithoutEnterprisePolicy];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Open the regular sign-in prompt from settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];

  // Enable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Sign-in from the regular prompt.
  CompleteSigninFlow();

  // Sync utilities require sync to be initialized in order to perform
  // operations on the Sync server.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:base::Seconds(10)];

  // Make sure the forced sign-in screen isn't shown because sign-in was
  // already done.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that intents are handled after forced sign-in is done when the app is
// opened.
- (void)testHandlingIntentAfterForcedSignin {
  // Serve the test page locally using the internal embedded server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&PageHttpResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  NSURL* URLToOpen = net::NSURLWithGURL(self.testServer->GetURL(kPageURL));

  // Add account.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Trigger a open URL external intent while the app is opened.
  SimulateExternalAppURLOpeningWithURL(URLToOpen);

  // Make sure that the page loading of the intent hasn't started yet.
  GREYAssertFalse([ChromeEarlGrey isLoading],
                  @"Page should not have been loaded yet");

  // Sign in account without enabling sync.
  WaitForForcedSigninScreenAndSignin(fakeIdentity1);

  // Make sure the forced sign-in screen isn't shown because it should have
  // been dismissed.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_nil()];

  // Verify that the intent was loaded (post sign-in prompt).
  WaitUntilPageLoadedWithURL(URLToOpen);
}

// Tests that intents are only handled when sign-in is done regardless of the
// type of sign-in prompt (regular or forced). This test chains the regular
// sign-in prompt and the forced sign-in prompt.
- (void)testHandlingIntentWhenSigninAfterSkippingRegularPrompt {
  // Serve the test page locally using the internal embedded server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&PageHttpResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  NSURL* URLToOpen = net::NSURLWithGURL(self.testServer->GetURL(kPageURL));

  // Restart the app to reset the policies.
  [self restartAppWithoutEnterprisePolicy];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Open the regular sign-in prompt from settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];

  // Enable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Simulate an external intent while the app is opened.
  SimulateExternalAppURLOpeningWithURL(URLToOpen);

  // Dismiss the regular sign-in prompt by skipping it.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebSigninSkipButtonMatcher()]
      performAction:grey_tap()];

  // Wait and verify that the forced sign-in screen is shown when the policy is
  // enabled and the browser is signed out.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];

  // Sign in account without enabling sync.
  WaitForForcedSigninScreenAndSignin(fakeIdentity);

  // Make sure the forced sign-in screen isn't shown because it should have
  // been dismissed.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_nil()];

  // Verify that the intent was loaded.
  WaitUntilPageLoadedWithURL(URLToOpen);
}

// Tests that intents are handled when sign-in is done from the regular sign-in
// prompt, where the forced sign-in prompt is skipped.
- (void)testHandlingIntentWhenSigninFromRegularPrompt {
  // Serve the test page locally using the internal embedded server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&PageHttpResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  NSURL* URLToOpen = net::NSURLWithGURL(self.testServer->GetURL(kPageURL));

  // Restart the app to reset the policies.
  [self restartAppWithoutEnterprisePolicy];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Open the regular sign-in prompt from settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];

  // Enable the forced sign-in policy while the regular sign-in prompt is
  // opened.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Simulate an external intent while the app is opened.
  SimulateExternalAppURLOpeningWithURL(URLToOpen);

  // Sign-in from the regular prompt.
  CompleteSigninFlow();

  // Sync utilities require sync to be initialized in order to perform
  // operations on the Sync server.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:base::Seconds(10)];

  // Make sure the forced sign-in screen isn't shown because the browser is
  // already signed in.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_nil()];

  // Verify that the intent was loaded.
  WaitUntilPageLoadedWithURL(URLToOpen);
}

// Tests that the sign-in prompt is shown on the other window when the window
// presenting the forced sign-in screen is closed.
- (void)testSigninScreenTransferToOtherWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  // Add account.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];

  // Open a new window on which the UIBlocker will be shown.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Close the window that is showing the forced sign-in screen which
  // corresponds to the first window that was opened.
  [ChromeEarlGrey closeWindowWithNumber:0];
  [ChromeEarlGrey waitForForegroundWindowCount:1];

  [EarlGrey setRootMatcherForSubsequentInteractions:nil];

  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests that the forced sign-in prompt, when there are multiple windows
// opened, can be shown on dynamic policy update when on an incognito browser
// tab.
- (void)testSignInScreenOnIncognitoWithMultiWindows {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  // TODO(crbug.com/40868899): Test is failing on iPad devices and simulator.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }

  // Restart the app to reset the policies.
  [self restartAppWithoutEnterprisePolicy];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Open an incognito tab in the first window.
  [ChromeEarlGrey openNewIncognitoTab];

  // Open a new window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Open an incognito tab in the second window. There should be incognito tabs
  // in both windows at this point; both should have the same test surface.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(1)];
  [ChromeEarlGrey openNewIncognitoTab];

  // Enable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Make sure that both windows will be considered when verifying for the
  // forced sign-in screen. This is done by removing the root matcher.
  [EarlGrey setRootMatcherForSubsequentInteractions:nil];

  // Wait and verify that the forced sign-in screen is shown when the policy is
  // enabled and the browser is signed out.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests that the forced sign-in prompt, when there are multiple windows
// opened, can be shown on dynamic policy update when on the tab switcher.
- (void)testSignInScreenOnTabSwitcherWithMultiWindows {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  // Restart the app to reset the policies.
  [self restartAppWithoutEnterprisePolicy];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Show the tab switcher of the first window.
  [ChromeEarlGrey showTabSwitcher];

  // Open a new window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Show the tab switcher of the second window.
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(1)];
  [ChromeEarlGrey showTabSwitcher];

  // Enable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Make sure that both windows will be considered when verifying for the
  // forced sign-in screen. This is done by removing the root matcher.
  [EarlGrey setRootMatcherForSubsequentInteractions:nil];

  // Wait and verify that the forced sign-in screen is shown when the policy is
  // enabled and the browser is signed out.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests that the forced sign-in prompt, when there are multiple windows
// opened, can be shown on dynamic policy update after cancelling the regular
// sign-in prompt. The policy is applied while the regular sign-in prompt is
// shown.
- (void)testSignInScreenOnRegularSigninPromptMultiWindows {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  // TODO(crbug.com/40210654).
  if ([ChromeEarlGrey isNewOverflowMenuEnabled]) {
    EARL_GREY_TEST_DISABLED(
        @"Earl Grey doesn't work properly with SwiftUI and multiwindow");
  }

  // Restart the app to reset the policies.
  [self restartAppWithoutEnterprisePolicy];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Open a new window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Show the regular sign-in prompt on the second window which will raise a UI
  // blocker on the second window.
  [ChromeEarlGreyUI openSettingsMenuInWindowWithNumber:1];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleSyncSettingsButton()];

  // Enable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Dismiss the regular sign-in prompt that is shown in the second window.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];

  // Make sure that both windows will be considered when verifying for the
  // forced sign-in screen. This is done by removing the root matcher.
  [EarlGrey setRootMatcherForSubsequentInteractions:nil];

  // Wait and verify that the forced sign-in screen is shown when the policy is
  // enabled and the browser is signed out.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests that the forced sign-in prompt can be shown on dynamic policy update
// when a browser modal is displayed on top of the browser view when there are
// multiple windows.
- (void)testSignInScreenOnModalMultiWindows {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  // TODO(crbug.com/40210654).
  if ([ChromeEarlGrey isNewOverflowMenuEnabled]) {
    EARL_GREY_TEST_DISABLED(
        @"Earl Grey doesn't work properly with SwiftUI and multiwindow");
  }

  // Restart the app to reset the policies.
  [self restartAppWithoutEnterprisePolicy];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Open a new window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Open the settings menu which represents a modal.
  [ChromeEarlGreyUI openSettingsMenuInWindowWithNumber:0];
  [ChromeEarlGreyUI openSettingsMenuInWindowWithNumber:1];

  // Make sure that both windows will be considered when verifying for the
  // forced sign-in screen. This is done by removing the root matcher.
  [EarlGrey setRootMatcherForSubsequentInteractions:nil];

  // Enable the forced sign-in policy to show the forced sign-in prompt.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Wait and verify that the forced sign-in screen is shown when the policy is
  // enabled and the browser is signed out.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests that the sign-in item in Google Service Settings can't be used when
// sign-in is forced by policy.
- (void)testGoogleServiceSettingsUI {
  // Add an identity to sign-in to enable the "Continue as ..." button in the
  // sign-in screen.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Enable forced sign-in and sign in from the prompt.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);
  ScrollToElementAndAssertVisibility(
      GetContinueButtonWithIdentityMatcher(fakeIdentity));
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      performAction:grey_tap()];

  // Open Google services settings and verify that sign-in item is greyed out.
  OpenGoogleServicesSettings();
  id<GREYMatcher> signinMatcher = GetItemMatcherWithTitleAndTextIDs(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_TEXT,
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_DETAIL);
  [[EarlGrey selectElementWithMatcher:signinMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Assert the sign-in item shows the "On" label in replacement of the toggle
  // switch.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_SETTING_ON))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Forced SignIn screen cannot be dismissed by the user swiping
// down on the view, as some other signin screens can be dismissed.
- (void)testSignInScreenCannotBeDismissedBySwipe {
  // Verify the signin screen appears.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_notNil()];
  // Swipe to dismiss the signin screen.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // Verify that the signin screen is still there.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_notNil()];
}

@end

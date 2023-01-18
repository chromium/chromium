// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/signin/ios/browser/features.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/test_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/browser/ui/first_run/field_trial_constants.h"
#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/mac/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForPageLoadTimeout;
using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsLink;
using chrome_test_util::SignOutAccountsButton;
using chrome_test_util::GoogleSyncSettingsButton;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;

namespace {

constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(5);

// Returns a matcher for the sign-in screen "Continue as <identity>" button.
id<GREYMatcher> GetContinueButtonWithIdentityMatcher(
    FakeSystemIdentity* fakeIdentity) {
  NSString* buttonTitle = l10n_util::GetNSStringF(
      IDS_IOS_FIRST_RUN_SIGNIN_CONTINUE_AS,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));

  return grey_allOf(grey_accessibilityLabel(buttonTitle),
                    grey_sufficientlyVisible(), nil);
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

// Opens the sign-out actions sheets from the account settings.
void OpenAccountSignOutActionsSheets() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];
}

// Signs out from the sign-out actions sheets UI. Will handle the data action
// sheet if `syncEnabled`.
void SignOutFromActionSheets(BOOL syncEnabled) {
  id<GREYMatcher> confirmationButtonMatcher = [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(confirmationButtonMatcher,
                                          grey_not(SignOutAccountsButton()),
                                          nil)] performAction:grey_tap()];

  if (syncEnabled) {
    confirmationButtonMatcher = [ChromeMatchersAppInterface
        buttonWithAccessibilityLabelID:
            IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON];
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(confirmationButtonMatcher,
                                            grey_not(SignOutAccountsButton()),
                                            nil)] performAction:grey_tap()];
  }
}

// Opens account settings and signs out from them.
void OpenAccountSettingsAndSignOut(BOOL syncEnabled) {
  OpenAccountSignOutActionsSheets();
  SignOutFromActionSheets(syncEnabled);
}

// Sets up the sign-in policy value dynamically at runtime.
void SetSigninEnterprisePolicyValue(BrowserSigninMode signinMode) {
  policy_test_utils::SetPolicy(static_cast<int>(signinMode),
                               policy::key::kBrowserSignin);
}

// Simulates opening `URL` from another application.
void SimulateExternalAppURLOpeningWithURL(NSURL* URL) {
  [ChromeEarlGreyAppInterface simulateExternalAppURLOpeningWithURL:URL];
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
}  // namespace

// Test the forced sign-in screens.
@interface ForcedSigninTestCase : ChromeTestCase

@end

@implementation ForcedSigninTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Configure the policy to force sign-in.
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(
      "<dict><key>BrowserSignin</key><integer>2</integer></dict>");
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  return config;
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
  [ChromeEarlGreyAppInterface signOutAndClearIdentities];
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
      grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(@"Cancel"),
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

// Tests that the sign-out action sheet has the right UI.
- (void)testSignOutActionSheetUI {
  // Add account.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Sign in account without enabling sync.
  WaitForForcedSigninScreenAndSignin(fakeIdentity1);

  // Make sure the forced sign-in screen isn't shown.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_nil()];

  // Open the SignOut menu actions sheets.
  OpenAccountSignOutActionsSheets();

  // Check the action sheet message and title that are exclusive to forced
  // sign-in.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_ENTERPRISE_FORCED_SIGNIN_SIGNOUT_DIALOG_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE))]
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
  OpenAccountSettingsAndSignOut(NO);

  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests signing out account from settings with sync enabled.
- (void)testSignOutFromAccountSettingSyncEnable {
  // Add account.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Sign in account without enabling sync.
  WaitForForcedSigninScreenAndSignin(fakeIdentity1);

  // Enable sync.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];

  OpenAccountSettingsAndSignOut(YES);

  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Test cancelling sign out from account settings.
- (void)testSignOutFromAccountSettingCancel {
  // Add account.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Sign in account without enabling sync.
  WaitForForcedSigninScreenAndSignin(fakeIdentity1);

  // Sign in and enable sync.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];

  OpenAccountSignOutActionsSheets();

  // Note that the iPad does not provide a CANCEL button by design. Click
  // anywhere on the screen to exit.
  [[[EarlGrey
      selectElementWithMatcher:grey_anyOf(chrome_test_util::CancelButton(),
                                          SignOutAccountsButton(), nil)]
      atIndex:1] performAction:grey_tap()];

  // Verify that the force sign-in screen isn't triggered when cancelling
  // sign-out.
  [[EarlGrey selectElementWithMatcher:GetForcedSigninScreenMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests signing out from sync settings.
- (void)testSignOutFromSyncSettings {
  // Add account.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Sign in.
  WaitForForcedSigninScreenAndSignin(fakeIdentity1);

  // Enable sync.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleSyncSettingsButton()];
  [[[EarlGrey selectElementWithMatcher:
                  grey_accessibilityLabel(l10n_util::GetNSString(
                      IDS_IOS_OPTIONS_ACCOUNTS_SIGN_OUT_TURN_OFF_SYNC))]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  SignOutFromActionSheets(YES);

  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests turning on sync for an account different from the one that is
// currently signed in.
- (void)testSignInWithOneAccountStartSyncWithAnotherAccount {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Tap on the account switcher and select `fakeIdentity1`..
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity1.userEmail)]
      performAction:grey_tap()];

  // Sign in account without enabling sync.
  WaitForForcedSigninScreenAndSignin(fakeIdentity1);

  // Open turn on sync dialog.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  // Select fakeIdentity2.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];

  // Check fakeIdentity2 is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity2];
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

// Tests that the sign-out footer has the right text when the user is syncing
// and forced sign-in is enabled.
- (void)testSignOutFooterForSignInAndSyncUserWithForcedSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign in from forced sign-in prompt and enable sync for the signed in
  // account.
  ScrollToElementAndAssertVisibility(
      GetContinueButtonWithIdentityMatcher(fakeIdentity));
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      performAction:grey_tap()];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:YES];

  // Open account settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Verify the content of the sign-out footer.
  NSString* footerText = [NSString
      stringWithFormat:
          @"%@\n\n%@",
          l10n_util::GetNSString(
              IDS_IOS_DISCONNECT_DIALOG_SYNCING_FOOTER_INFO_MOBILE),
          l10n_util::GetNSString(
              IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE_WITH_LEARN_MORE)];
  footerText = [footerText stringByReplacingOccurrencesOfString:@"BEGIN_LINK"
                                                     withString:@""];
  footerText = [footerText stringByReplacingOccurrencesOfString:@"END_LINK"
                                                     withString:@""];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(footerText),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the forced sign-in prompt can be shown on dynamic policy update
// when a browser modal is displayed on top of the browser view.
- (void)testSignInScreenOnModal {
  // Restart the app to reset the policies.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

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
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

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
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

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
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Open the regular sign-in prompt from settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];

  // Enable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Dismiss the regular sign-in prompt by skipping it.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];

  // Wait and verify that the forced sign-in screen is shown when the policy is
  // enabled and the browser is signed out.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
}

// Tests that the forced sign-in prompt isn't shown when sign-in is done from
// the regular sign-in prompt.
- (void)testNoSignInScreenWhenSigninFromRegularSigninPrompt {
  // Restart the app to reset the policies.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Open the regular sign-in prompt from settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];

  // Enable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Sign-in from the regular prompt.
  [SigninEarlGreyUI tapSigninConfirmationDialog];

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
  GREYAssertFalse([ChromeEarlGreyAppInterface isLoading],
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
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Open the regular sign-in prompt from settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];

  // Enable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Simulate an external intent while the app is opened.
  SimulateExternalAppURLOpeningWithURL(URLToOpen);

  // Dismiss the regular sign-in prompt by skipping it.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
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
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Open the regular sign-in prompt from settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];

  // Enable the forced sign-in policy while the regular sign-in prompt is
  // opened.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Simulate an external intent while the app is opened.
  SimulateExternalAppURLOpeningWithURL(URLToOpen);

  // Sign-in from the regular prompt.
  [SigninEarlGreyUI tapSigninConfirmationDialog];

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

// Tests that chaining the regular sign-in prompt and the forced sign-in screen
// is done correctly when the forced sign-in policy is enabled and an external
// intent is triggered while the advanced settings are shown. This test makes
// sure that having the browser signed in isn't sufficient to start loading the
// intent where the sign-in prompt should be manually dismissed first before
// doing that. The account will be signed in temporarily when showing advanced
// settings.
- (void)testHandlingIntentWhenSigninAfterSyncSettingOnRegularPrompt {
  // Serve the test page locally using the internal embedded server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&PageHttpResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  NSURL* URLToOpen = net::NSURLWithGURL(self.testServer->GetURL(kPageURL));

  // Restart the app to reset the policies.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Disable the forced sign-in policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kEnabled);

  // Dismiss the forced sign-in screen if presented. This may happen sometimes
  // if the browser has the forced sign-in policy enabled at start time.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Open the regular sign-in prompt from settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];

  // Open advanced sync settings.
  [[EarlGrey selectElementWithMatcher:SettingsLink()] performAction:grey_tap()];
  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(
                         kManageSyncTableViewAccessibilityIdentifier)];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Enable the forced sign-in policy while the advanced settings are opened.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kForced);

  // Simulate an external intent while the advanced settings are opened.
  SimulateExternalAppURLOpeningWithURL(URLToOpen);

  // Verify that the advanced settings are still there. This verifies that the
  // sign-in prompt isn't dismissed when the policy becomes active.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Dismiss advanced sync settings.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Dismiss the regular sign-in prompt by skipping it.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
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

// Tests that signing out from sync settings will trigger showing the forced
// sign-in screen in one of the foregrounded window (when multi windows).
- (void)testSignOutFromSyncSettingsWithMultiWindows {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  // Add account.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];

  // Sign in.
  WaitForForcedSigninScreenAndSignin(fakeIdentity1);

  // Open a new window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(0)];

  // Sign out account from account settings.
  OpenAccountSettingsAndSignOut(/*syncEnabled=*/NO);

  // Wait and verify that the forced sign-in screen is shown.
  [ChromeEarlGrey waitForMatcher:GetForcedSigninScreenMatcher()];
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

  // TODO(crbug.com/1369148): Test is failing on iPad devices and simulator.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }

  // Restart the app to reset the policies.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

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
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

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

  // TODO(crbug.com/1285974).
  if ([ChromeEarlGrey isNewOverflowMenuEnabled]) {
    EARL_GREY_TEST_DISABLED(
        @"Earl Grey doesn't work properly with SwiftUI and multiwindow");
  }

  // Restart the app to reset the policies.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

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
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];

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

  // TODO(crbug.com/1285974).
  if ([ChromeEarlGrey isNewOverflowMenuEnabled]) {
    EARL_GREY_TEST_DISABLED(
        @"Earl Grey doesn't work properly with SwiftUI and multiwindow");
  }

  // Restart the app to reset the policies.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

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

@end

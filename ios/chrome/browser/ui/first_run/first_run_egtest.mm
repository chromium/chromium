// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/signin/ios/browser/features.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager_constants.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service_constants.h"
#import "ios/third_party/earl_grey2/src/CommonLib/Matcher/GREYLayoutConstraint.h"  // nogncheck

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::AdvancedSyncSettingsDoneButtonMatcher;

namespace {

NSString* const kBeginBoldTag = @"BEGIN_BOLD[ \t]*";
NSString* const kEndBoldTag = @"[ \t]*END_BOLD";

// Returns a matcher for the welcome screen accept button.
id<GREYMatcher> GetAcceptButton() {
  return grey_allOf(grey_text(l10n_util::GetNSString(
                        IDS_IOS_FIRST_RUN_WELCOME_SCREEN_ACCEPT_BUTTON)),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the button to open the Sync settings.
id<GREYMatcher> GetSyncSettings() {
  id<GREYMatcher> disclaimer =
      grey_accessibilityID(kPromoStyleDisclaimerViewAccessibilityIdentifier);
  return grey_allOf(grey_accessibilityLabel(@"settings"),
                    grey_ancestor(disclaimer), nil);
}

// Returns a matcher for the button to add account.
id<GREYMatcher> GetAddAccountButton() {
  return grey_allOf(grey_text(l10n_util::GetNSString(
                        IDS_IOS_ACCOUNT_UNIFIED_CONSENT_ADD_ACCOUNT)),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the button to sign-in and sync (OLD string).
id<GREYMatcher> GetYesImInButton() {
  return grey_allOf(grey_text(l10n_util::GetNSString(
                        IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON)),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the button to skip sign-in and sync (OLD string).
id<GREYMatcher> GetNoThanksButton() {
  return grey_allOf(
      grey_text(l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECONDARY_ACTION)),
      grey_sufficientlyVisible(), nil);
}

// Returns a constraint where the element is below the reference.
GREYLayoutConstraint* BelowConstraint() {
  return [GREYLayoutConstraint
      layoutConstraintWithAttribute:kGREYLayoutAttributeTop
                          relatedBy:kGREYLayoutRelationGreaterThanOrEqual
               toReferenceAttribute:kGREYLayoutAttributeBottom
                         multiplier:1.0
                           constant:0.0];
}

}  // namespace

// Test first run stages
@interface FirstRunTestCase : ChromeTestCase

@end

@implementation FirstRunTestCase

- (void)setUp {
  [[self class] testForStartup];

  [super setUp];
  [FirstRunAppInterface setUMACollectionEnabled:NO];
  [FirstRunAppInterface resetUMACollectionEnabledByDefault];
}

- (void)tearDown {
  [PolicyAppInterface clearPolicies];
  [FirstRunAppInterface setUMACollectionEnabled:NO];
  [FirstRunAppInterface resetUMACollectionEnabledByDefault];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(signin::kNewMobileIdentityConsistencyFRE);
  config.features_disabled.push_back(kEnableFREDefaultBrowserPromoScreen);

  // Show the First Run UI at startup.
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");

  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByKilling;

  return config;
}

#pragma mark - Helpers

// Remove when default browser screen will be fully enabled
- (BOOL)isDefaultBrowserTestDisabled {
  return YES;
}

// Checks that the welcome screen is displayed.
- (void)verifyWelcomeScreenIsDisplayed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunWelcomeScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Checks that the sign-in & sync screen is displayed.
- (void)verifySignInSyncScreenIsDisplayed {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSigninSyncScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Checks that the forced sign-in screen is displayed.
- (void)verifyForcedSigninScreenIsDisplayed {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunLegacySignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Checks that the default browser screen is displayed.
- (void)verifyDefaultBrowserScreenIsDisplayed {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Checks that none of any FRE's screen is displayed.
- (void)verifyFREIsDismissed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunWelcomeScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSigninSyncScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Scrolls down to `elementMatcher` in the scrollable content of the first run
// screen.
- (void)scrollToElementAndAssertVisibility:(id<GREYMatcher>)elementMatcher {
  id<GREYMatcher> scrollView =
      grey_accessibilityID(kPromoStyleScrollViewAccessibilityIdentifier);

  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(elementMatcher,
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:scrollView] assertWithMatcher:grey_notNil()];
}

- (void)toggleSwitchWithIdentifier:(NSString*)identifier
                           toValue:(BOOL)toggleOn {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          identifier,
                                          /*is_toggled_on=*/!toggleOn,
                                          /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(toggleOn)];
}

- (void)verifySwitchWithIdentifier:(NSString*)identifier
                           toValue:(BOOL)toggleOn {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          identifier,
                                          /*is_toggled_on=*/toggleOn,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_notNil()];
}

#pragma mark - Welcome Screen Tests

// Checks that the Welcome screen is displayed correctly.
- (void)testWelcomeScreenUI {
  [self verifyWelcomeScreenIsDisplayed];

  // Validate the Title text.
  NSString* expectedTitleText =
      [ChromeEarlGrey isIPadIdiom]
          ? l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPAD)
          : l10n_util::GetNSString(
                IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPHONE);
  id<GREYMatcher> title = grey_text(expectedTitleText);
  [self scrollToElementAndAssertVisibility:title];

  // Validate the Subtitle text.
  id<GREYMatcher> subtitle = grey_text(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_SUBTITLE));
  [self scrollToElementAndAssertVisibility:subtitle];

  // Validate the Accept box.
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
}

// Checks that the Welcome screen is displayed correctly when enterprise is
// enabled.
- (void)testWelcomeScreenUIForEnterprise {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;

  // Configure the policy to force sign-in.
  std::string policy_data = "<dict>"
                            "    <key>BrowserSignin</key>"
                            "    <integer>2</integer>"
                            "</dict>";
  base::RemoveChars(policy_data, base::kWhitespaceASCII, &policy_data);

  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(policy_data);

  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self verifyWelcomeScreenIsDisplayed];

  // Validate the Title text.
  id<GREYMatcher> title = grey_text(l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_ENTERPRISE));
  [self scrollToElementAndAssertVisibility:title];

  // Validate the Subtitle text.
  id<GREYMatcher> subtitle = grey_text(l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_WELCOME_SCREEN_SUBTITLE_ENTERPRISE));
  [self scrollToElementAndAssertVisibility:subtitle];

  // Validate the Managed text.
  id<GREYMatcher> managed = grey_text(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_MANAGED));
  [self scrollToElementAndAssertVisibility:managed];

  // Validate the Accept box.
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
}

#pragma mark - Sign-in & Sync Tests

// Checks that the sign-in & sync screen is displayed correctly with no account
// (using OLD strings set).
- (void)testSignInSyncScreenUIOldStringNoAccount {
  [self verifyWelcomeScreenIsDisplayed];

  // Go to the sign-in & sync screen.
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInSyncScreenIsDisplayed];

  // Validate the Title text.
  id<GREYMatcher> title =
      grey_text(l10n_util::GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE));
  [self scrollToElementAndAssertVisibility:title];

  // Validate the Subtitle text.
  id<GREYMatcher> subtitle = grey_text(
      l10n_util::GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_TITLE));
  [self scrollToElementAndAssertVisibility:subtitle];

  // Validate the Primary button text.
  [self scrollToElementAndAssertVisibility:GetAddAccountButton()];

  // Validate the Secondary button text.
  [self scrollToElementAndAssertVisibility:GetNoThanksButton()];

  // Validate that the sync button is not interactible.
  [[EarlGrey selectElementWithMatcher:grey_allOf(GetSyncSettings(),
                                                 grey_interactable(), nil)]
      assertWithMatcher:grey_nil()];
}

// Checks that the sign-in & sync screen is displayed correctly with an account
// (using OLD strings set).
- (void)testSignInSyncScreenUIOldString {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self verifyWelcomeScreenIsDisplayed];

  // Go to the sign-in & sync screen.
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInSyncScreenIsDisplayed];

  // Validate the Title text.
  id<GREYMatcher> title =
      grey_text(l10n_util::GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE));
  [self scrollToElementAndAssertVisibility:title];

  // Validate the Subtitle text.
  id<GREYMatcher> subtitle = grey_text(
      l10n_util::GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_TITLE));
  [self scrollToElementAndAssertVisibility:subtitle];

  // Validate the Primary button text.
  [self scrollToElementAndAssertVisibility:GetYesImInButton()];

  // Validate the Secondary button text.
  [self scrollToElementAndAssertVisibility:GetNoThanksButton()];

  // Validate that the sync button is interactible.
  [[EarlGrey selectElementWithMatcher:grey_allOf(GetSyncSettings(),
                                                 grey_interactable(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the forced sign-in screen is shown when the policy is enabled.
// If the user says no during the FRE, then they should be re-prompted at the
// end of the FRE.
- (void)testSignInScreenUIWhenForcedByPolicy {
  AppLaunchConfiguration configToSetPolicy = self.appConfigurationForTestCase;

  // Configure the policy to force sign-in.
  std::string policy_data = "<dict>"
                            "    <key>BrowserSignin</key>"
                            "    <integer>2</integer>"
                            "</dict>";
  base::RemoveChars(policy_data, base::kWhitespaceASCII, &policy_data);

  configToSetPolicy.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  configToSetPolicy.additional_args.push_back(policy_data);

  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:configToSetPolicy];

  // Add account for the identity switcher to be shown.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Go to the sign-in & sync screen from the welcome screen.
  [self verifyWelcomeScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  // Sanity check that the sign-in & sync screen is being displayed.
  [self verifySignInSyncScreenIsDisplayed];

  // Do not sign-in or sync.
  [self scrollToElementAndAssertVisibility:GetNoThanksButton()];
  [[EarlGrey selectElementWithMatcher:GetNoThanksButton()]
      performAction:grey_tap()];

  // Add account for the identity switcher to be shown.
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self verifyForcedSigninScreenIsDisplayed];

  // Restart the app to reset the policies and to make sure that the forced
  // sign-in UI isn't retriggered when tearing down.
  AppLaunchConfiguration configToCleanPolicy;
  configToCleanPolicy.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:configToCleanPolicy];
}

// Checks that the default browser screen is displayed correctly.
// TODO(crbug.com/1282248): Re-enable this test.
- (void)DISABLED_testDefaultBrowserScreenUI {
  if ([self isDefaultBrowserTestDisabled]) {
    return;
  }

  // Go to the default browser screen.
  [self verifyWelcomeScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInSyncScreenIsDisplayed];
  [[EarlGrey selectElementWithMatcher:GetNoThanksButton()]
      performAction:grey_tap()];

  [self verifyDefaultBrowserScreenIsDisplayed];

  // Validate the Title text.
  id<GREYMatcher> title = grey_text(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE));
  [self scrollToElementAndAssertVisibility:title];

  // Validate the Subtitle text.
  id<GREYMatcher> subtitle = grey_text(l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE));
  [self scrollToElementAndAssertVisibility:subtitle];

  // Remove bold tags in instructions.
  StringWithTag firstInstructionParsed = ParseStringWithTag(
      l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_FIRST_STEP),
      kBeginBoldTag, kEndBoldTag);
  StringWithTag secondInstructionParsed = ParseStringWithTag(
      l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECOND_STEP),
      kBeginBoldTag, kEndBoldTag);
  StringWithTag thirdInstructionParsed = ParseStringWithTag(
      l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_THIRD_STEP),
      kBeginBoldTag, kEndBoldTag);

  // Verify instruction order.
  id<GREYMatcher> firstInstruction = grey_text(firstInstructionParsed.string);
  id<GREYMatcher> secondInstruction = grey_text(secondInstructionParsed.string);
  id<GREYMatcher> thirdInstruction = grey_text(thirdInstructionParsed.string);

  // Scroll to ensure that the third instruction is visible.
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kPromoStyleScrollViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:thirdInstruction]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:scrollViewMatcher];

  [[EarlGrey selectElementWithMatcher:secondInstruction]
      assertWithMatcher:grey_layout(@[ BelowConstraint() ], firstInstruction)];
  [[EarlGrey selectElementWithMatcher:thirdInstruction]
      assertWithMatcher:grey_layout(@[ BelowConstraint() ], secondInstruction)];
}

// Navigates to the Terms of Service and back.
- (void)testTermsAndConditions {
  // Tap on “Terms of Service” on the first screen
  [self verifyWelcomeScreenIsDisplayed];

  // Scroll to and open the ToS screen.
  id<GREYMatcher> termsOfServiceLink =
      grey_accessibilityLabel(@"Terms of Service");
  [self scrollToElementAndAssertVisibility:termsOfServiceLink];
  [[EarlGrey selectElementWithMatcher:termsOfServiceLink]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_FIRSTRUN_TERMS_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on “Done” on the ToS screen
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Ensure we went back to the First Run screen.
  [self verifyWelcomeScreenIsDisplayed];

  // Scroll to and tap the accept ToS button.
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInSyncScreenIsDisplayed];
}

// Tests that the FRE is shown when incognito is forced by policy.
- (void)testFirstRunWithIncognitoForced {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;

  std::string policy_data = "<dict>"
                            "    <key>IncognitoModeAvailability</key>"
                            "    <integer>2</integer>"
                            "</dict>";
  base::RemoveChars(policy_data, base::kWhitespaceASCII, &policy_data);

  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(policy_data);

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Verify that the FRE UI is shown while the browser is in incognito mode.
  [self verifyWelcomeScreenIsDisplayed];
}

// Tests that the FRE sign in screen is not displayed when sign in is disabled
// by policy.
- (void)testSignInDisabled {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;

  // Configure the policy to disable SignIn.
  std::string policy_data = "<dict>"
                            "    <key>BrowserSignin</key>"
                            "    <integer>0</integer>"
                            "</dict>";
  base::RemoveChars(policy_data, base::kWhitespaceASCII, &policy_data);

  config.additional_args.push_back("-EnableSamplePolicies");
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(policy_data);

  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self verifyWelcomeScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];
  [self verifyFREIsDismissed];
}

// Checks that when opening the app no accounts are here and the primary button
// allows to create a new account and that it is updated if a new account is
// added.
- (void)testAddAccount {
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:GetAddAccountButton()]
      performAction:grey_tap()];

  // Check for the fake SSO screen.
  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(kFakeAddAccountViewIdentifier)];
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
  [[EarlGrey selectElementWithMatcher:GetYesImInButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:GetAddAccountButton()]
      assertWithMatcher:grey_nil()];
}

// Checks that it is possible to add an account even if there is already account
// and that it is possible to switch accounts when multiple accounts are
// present.
- (void)testSignInSelectAccount {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  id<GREYMatcher> identityButton =
      grey_accessibilityID(kIdentityButtonControlIdentifier);
  [self scrollToElementAndAssertVisibility:identityButton];
  [[EarlGrey selectElementWithMatcher:identityButton] performAction:grey_tap()];

  // Check that `fakeIdentity2` is displayed.
  [self scrollToElementAndAssertVisibility:IdentityCellMatcherForEmail(
                                               fakeIdentity2.userEmail)];
  // Check that 'Add Account' is displayed.
  [self scrollToElementAndAssertVisibility:
            grey_accessibilityLabel(l10n_util::GetNSString(
                IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_ADD_ACCOUNT))];

  // Select `fakeIdentity2`.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      performAction:grey_tap()];

  // Check that the title of the primary button updates for `fakeIdentity2`.
  [self scrollToElementAndAssertVisibility:GetYesImInButton()];
}

// Checks that the user is signed in and that sync is turned on after the user
// chooses to turn on sync.
- (void)testSignInAndTurnOnSync {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInSyncScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetYesImInButton()];
  [[EarlGrey selectElementWithMatcher:GetYesImInButton()]
      performAction:grey_tap()];

  // Verify that the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Verify that the sync cell is visible and "On" is displayed.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:YES];

  // Close opened settings for proper tear down.
  [[self class] removeAnyOpenMenusAndInfoBars];
}

// Checks that pressing "No thanks" on sign-in & sync screen doesn't sign in the
// user and doesn't sync.
- (void)testNoSignInNoSync {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInSyncScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetNoThanksButton()];
  [[EarlGrey selectElementWithMatcher:GetNoThanksButton()]
      performAction:grey_tap()];

  // Verify that the user is not signed in.
  [SigninEarlGrey verifySignedOut];

  [ChromeEarlGreyUI openSettingsMenu];

  // Because the user is not signed in, the sync cell is not be visible.
  [SigninEarlGrey verifySyncUIIsHidden];

  // Close opened settings for proper tear down.
  [[self class] removeAnyOpenMenusAndInfoBars];
}

// The browser should only be signed in temporarily while the advanced settings
// prompt is opened and then signed out when the user selects "No thanks".
// Sync is also turned off.
- (void)testAdvancedSettingsSignoutSyncOff {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self verifyWelcomeScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInSyncScreenIsDisplayed];
  [[EarlGrey selectElementWithMatcher:GetSyncSettings()]
      performAction:grey_tap()];

  // Check that Sync hasn't started yet, allowing the user to change some
  // settings.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't have finished its original setup yet");

  [self scrollToElementAndAssertVisibility:
            AdvancedSyncSettingsDoneButtonMatcher()];
  [[EarlGrey selectElementWithMatcher:AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];

  // Check sync did not start yet.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't start when discarding advanced settings.");

  [self scrollToElementAndAssertVisibility:GetNoThanksButton()];
  [[EarlGrey selectElementWithMatcher:GetNoThanksButton()]
      performAction:grey_tap()];

  // Verify that the browser isn't signed in by validating that there isn't a
  // sync cell visible in settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIIsHidden];

  // Close opened settings for proper tear down.
  [[self class] removeAnyOpenMenusAndInfoBars];
}

// If browser is already signed in and the user opens the advanced settings then
// selects "No thanks", the user should stay signed in, but sync should be
// turned off.
- (void)testAdvancedSettingsSignedInSyncOff {
  // Sign-in browser.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  // Reload with forced first run enabled.
  AppLaunchConfiguration config = self.appConfigurationForTestCase;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  // Add the switch to make sure that fakeIdentity1 is known at startup to avoid
  // automatic sign out.
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kSignInAtStartup);
  config.additional_args.push_back(
      std::string("-") + test_switches::kAddFakeIdentitiesAtStartup + "=" +
      [FakeSystemIdentity encodeIdentitiesToBase64:@[ fakeIdentity ]]);

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self verifyWelcomeScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInSyncScreenIsDisplayed];
  [[EarlGrey selectElementWithMatcher:GetSyncSettings()]
      performAction:grey_tap()];

  // Check that Sync hasn't started yet, allowing the user to change some
  // settings.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't have finished its original setup yet");

  [self scrollToElementAndAssertVisibility:
            AdvancedSyncSettingsDoneButtonMatcher()];
  [[EarlGrey selectElementWithMatcher:AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];

  // Check sync did not start yet.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't start when discarding advanced settings.");

  [self scrollToElementAndAssertVisibility:GetNoThanksButton()];
  [[EarlGrey selectElementWithMatcher:GetNoThanksButton()]
      performAction:grey_tap()];

  // Verify that the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Verify that the sync cell is visible and "Off" is displayed.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:NO];

  // Close opened settings for proper tear down.
  [[self class] removeAnyOpenMenusAndInfoBars];
}

// Checks that sync is turned on after the user chose to turn on sync in the
// advanced sync settings screen and that the correct sync options are selected.
- (void)testCustomSyncOn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self verifyWelcomeScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInSyncScreenIsDisplayed];
  [[EarlGrey selectElementWithMatcher:GetSyncSettings()]
      performAction:grey_tap()];

  // Check that Sync hasn't started yet, allowing the user to change some
  // settings.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't have finished its original setup yet");

  // Toggle OFF Sync Everything and History.
  [self toggleSwitchWithIdentifier:kSyncEverythingItemAccessibilityIdentifier
                           toValue:NO];
  [self toggleSwitchWithIdentifier:kSyncOmniboxHistoryIdentifier toValue:NO];

  [self scrollToElementAndAssertVisibility:
            AdvancedSyncSettingsDoneButtonMatcher()];
  [[EarlGrey selectElementWithMatcher:AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];

  // Check sync did not start yet.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't start when discarding advanced settings.");

  [self scrollToElementAndAssertVisibility:GetYesImInButton()];
  [[EarlGrey selectElementWithMatcher:GetYesImInButton()]
      performAction:grey_tap()];

  // Check sync did start.
  GREYAssertTrue([FirstRunAppInterface isSyncFirstSetupComplete],
                 @"Sync should start when turning on sync in FRE.");

  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySyncUIEnabled:YES];

  // Go to the sync settings.
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ManageSyncSettingsButton()];

  // Check that the correct sync options are toggled OFF.
  [self verifySwitchWithIdentifier:kSyncEverythingItemAccessibilityIdentifier
                           toValue:NO];
  [self verifySwitchWithIdentifier:kSyncOmniboxHistoryIdentifier toValue:NO];

  // Revert back sync options to Sync Everything ON.
  [self toggleSwitchWithIdentifier:kSyncEverythingItemAccessibilityIdentifier
                           toValue:YES];

  // Close opened settings for proper tear down.
  [[self class] removeAnyOpenMenusAndInfoBars];
}

// Checks that the user is signed in, but no sync options is selected.
- (void)testCustomSyncOff {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self verifyWelcomeScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInSyncScreenIsDisplayed];
  [[EarlGrey selectElementWithMatcher:GetSyncSettings()]
      performAction:grey_tap()];

  // Check that Sync hasn't started yet, allowing the user to change some
  // settings.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't have finished its original setup yet");

  // Turn OFF sync.
  NSArray* switchesIdentifier = @[
    kSyncEverythingItemAccessibilityIdentifier,
    kSyncAutofillIdentifier,
    kSyncBookmarksIdentifier,
    kSyncOmniboxHistoryIdentifier,
    kSyncOpenTabsIdentifier,
    kSyncPasswordsIdentifier,
    kSyncReadingListIdentifier,
    kSyncPreferencesIdentifier,
  ];
  for (NSString* identifier in switchesIdentifier) {
    [self toggleSwitchWithIdentifier:identifier toValue:NO];
  }

  [self scrollToElementAndAssertVisibility:
            AdvancedSyncSettingsDoneButtonMatcher()];
  [[EarlGrey selectElementWithMatcher:AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];

  // Check sync did not start yet.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't start when discarding advanced settings.");

  [self scrollToElementAndAssertVisibility:GetYesImInButton()];
  [[EarlGrey selectElementWithMatcher:GetYesImInButton()]
      performAction:grey_tap()];

  // Check sync did start.
  GREYAssertTrue([FirstRunAppInterface isSyncFirstSetupComplete],
                 @"Sync should start when turning on sync in FRE.");

  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Go to the sync settings.
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ManageSyncSettingsButton()];

  // Check that the all sync options are toggled OFF.
  for (NSString* identifier in switchesIdentifier) {
    [self verifySwitchWithIdentifier:identifier toValue:NO];
  }

  // Revert back sync options to Sync Everything ON.
  [self toggleSwitchWithIdentifier:kSyncEverythingItemAccessibilityIdentifier
                           toValue:YES];

  // Close opened settings for proper tear down.
  [[self class] removeAnyOpenMenusAndInfoBars];
}

// Checks that the user is not signed in and that sync is turned off after the
// user chose to not sign-in even though they selected some sync options in the
// advanced sync settings screen.
- (void)testCustomSyncSignout {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self verifyWelcomeScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInSyncScreenIsDisplayed];
  [[EarlGrey selectElementWithMatcher:GetSyncSettings()]
      performAction:grey_tap()];

  // Check that Sync hasn't started yet, allowing the user to change some
  // settings.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't have finished its original setup yet");

  // Toggle OFF Sync Everything and History.
  [self toggleSwitchWithIdentifier:kSyncEverythingItemAccessibilityIdentifier
                           toValue:NO];
  [self toggleSwitchWithIdentifier:kSyncOmniboxHistoryIdentifier toValue:NO];

  [self scrollToElementAndAssertVisibility:
            AdvancedSyncSettingsDoneButtonMatcher()];
  [[EarlGrey selectElementWithMatcher:AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];

  // Check sync did not start yet.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't start when discarding advanced settings.");

  // Do not sign-in/sync.
  [self scrollToElementAndAssertVisibility:GetNoThanksButton()];
  [[EarlGrey selectElementWithMatcher:GetNoThanksButton()]
      performAction:grey_tap()];

  // Verify that the browser isn't signed in by validating that there isn't a
  // sync cell visible in settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIIsHidden];

  // Revert back sync options to Sync Everything ON.
  [[self class] removeAnyOpenMenusAndInfoBars];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ManageSyncSettingsButton()];
  [self toggleSwitchWithIdentifier:kSyncEverythingItemAccessibilityIdentifier
                           toValue:YES];

  // Close opened settings for proper tear down.
  [[self class] removeAnyOpenMenusAndInfoBars];
}

// Checks that the sync screen doesn't appear when the SyncDisabled policy is
// enabled.
- (void)testSyncDisabled {
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);

  // Go to the sign-in screen.
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  // The Sync screen should not be displayed, so the NTP should be visible.
  [self verifyFREIsDismissed];
}

@end

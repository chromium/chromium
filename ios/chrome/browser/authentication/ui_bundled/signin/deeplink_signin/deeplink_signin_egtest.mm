// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/policy/policy_constants.h"
#import "components/signin/public/base/signin_deep_link_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/test/signin_matchers.h"
#import "ios/chrome/browser/authentication/ui_bundled/views/views_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::FullscreenSigninPrimaryButtonMatcher;
using chrome_test_util::GREYAssertErrorNil;
using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using signin_metrics::CrossDeviceInitialState;

namespace {

constexpr char kCrossDeviceSigninUrl[] =
    "https://www.google.com/chrome/go-mobile";

// Returns the deep link URL for the given `email`.
NSURL* GetDeepLinkURLForEmail(NSString* email) {
  NSString* urlString =
      [NSString stringWithFormat:@"%s?email=%@&entry_point_id=1",
                                 kCrossDeviceSigninUrl, email];
  return [NSURL URLWithString:urlString];
}

void CheckMetrics(signin::ExternalEntryPoint entryPoint,
                  int initialAccountsNumber,
                  CrossDeviceInitialState initialState) {
  NSString* entryPointSuffix =
      entryPoint == signin::ExternalEntryPoint::kDesktopDefault
          ? @"DesktopDefault"
          : @"Unknown";
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:initialAccountsNumber
                         forHistogram:
                             @"Signin.CrossDevice.InitialAccountsNumber"],
      @"Failed to record InitialAccountsNumber");
  NSString* initialAccountsHistogram =
      [NSString stringWithFormat:@"Signin.CrossDevice.InitialAccountsNumber.%@",
                                 entryPointSuffix];
  GREYAssertNil([MetricsAppInterface
                    expectUniqueSampleWithCount:1
                                      forBucket:initialAccountsNumber
                                   forHistogram:initialAccountsHistogram],
                @"Failed to record InitialAccountsNumber");
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(initialState)
                         forHistogram:@"Signin.CrossDevice.InitialState"],
      @"Failed to record InitialState");
  NSString* initialStateHistogram = [NSString
      stringWithFormat:@"Signin.CrossDevice.InitialState.%@", entryPointSuffix];
  GREYAssertNil([MetricsAppInterface
                    expectUniqueSampleWithCount:1
                                      forBucket:static_cast<int>(initialState)
                                   forHistogram:initialStateHistogram],
                @"Failed to record InitialState");
}

void CheckAccountSignin(FakeSystemIdentity* chosenIdentity,
                        int initialAccountsNumber,
                        CrossDeviceInitialState initialState) {
  [ChromeEarlGrey waitForMatcher:chrome_test_util::SigninScreenPromoMatcher()];

  id<GREYMatcher> primaryButton =
      FullscreenSigninPrimaryButtonMatcher(chosenIdentity);
  [[EarlGrey
      selectElementWithMatcher:StaticTextWithAccessibilityLabelId(
                                   IDS_IOS_UNO_UPGRADE_PROMO_SIGNIN_TITLE_1)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:StaticTextWithAccessibilityLabelId(
                                          IDS_IOS_DEEPLINK_SIGNIN_SUBTITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:primaryButton]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_FIRST_RUN_SIGNIN_STAY_SIGNED_OUT)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:primaryButton] performAction:grey_tap()];
  // Verify that `chosenIdentity` is now signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:chosenIdentity];
  CheckMetrics(signin::ExternalEntryPoint::kDesktopDefault,
               initialAccountsNumber, initialState);
}

void CheckAccountSwitch(FakeSystemIdentity* signedInIdentity,
                        FakeSystemIdentity* chosenIdentity,
                        int initialAccountsNumber,
                        CrossDeviceInitialState initialState) {
  [ChromeEarlGrey waitForMatcher:chrome_test_util::SigninScreenPromoMatcher()];

  id<GREYMatcher> primaryButton =
      FullscreenSigninPrimaryButtonMatcher(chosenIdentity);
  [[EarlGrey
      selectElementWithMatcher:StaticTextWithAccessibilityLabelId(
                                   IDS_IOS_DEEPLINK_ACCOUNT_SWITCH_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
  NSString* expectedSubtitle = l10n_util::GetNSStringF(
      IDS_IOS_DEEPLINK_ACCOUNT_SWITCH_SUBTITLE,
      base::SysNSStringToUTF16(signedInIdentity.userEmail),
      base::SysNSStringToUTF16(chosenIdentity.userEmail));
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(expectedSubtitle)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:primaryButton]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_DEEPLINK_ACCOUNT_SWITCH_NO_THANKS)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:primaryButton] performAction:grey_tap()];
  // Verify that `chosenIdentity` is now signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:chosenIdentity];
  CheckMetrics(signin::ExternalEntryPoint::kDesktopDefault,
               initialAccountsNumber, initialState);
}

}  // namespace

// Tests for Deeplink Sign-in.
@interface DeeplinkSigninTestCase : ChromeTestCase
@end

@implementation DeeplinkSigninTestCase

- (void)setUp {
  [super setUp];
  GREYAssertErrorNil([MetricsAppInterface setupHistogramTester]);
}

- (void)tearDownHelper {
  [ChromeEarlGrey resetDataForLocalStatePref:prefs::kSigninAllowedOnDevice];
  GREYAssertErrorNil([MetricsAppInterface releaseHistogramTester]);

  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled_and_params.push_back(
      {switches::kCrossDeviceSignin,
       {{switches::kCrossDeviceSigninUrl.name, kCrossDeviceSigninUrl}}});
  return config;
}

// Tests that opening a cross-device sign-in deep link for an account that is
// already signed in shows the "already signed in" snackbar and does not show
// the sign-in flow.
- (void)testCrossDeviceSigninAlreadySignedIn {
  // Sign in with a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Simulate opening the URL from an external app.
  [ChromeEarlGrey
      simulateExternalAppURLOpeningWithURL:GetDeepLinkURLForEmail(
                                               fakeIdentity.userEmail)];

  // Verify that the "already signed in" snackbar is shown.
  NSString* expectedMessage = l10n_util::GetNSStringF(
      IDS_IOS_DEEPLINK_SIGNIN_ALREADY_SIGNED_IN_DESCRIPTION,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));
  [SigninEarlGreyUI dismissSigninConfirmationSnackbarWithTitle:expectedMessage
                                                 assertVisible:YES];
}

// Tests that opening a cross-device sign-in deep link for an account that is
// not signed in shows the sign-in flow.
- (void)testCrossDeviceSigninSignedOutTargetAccountOnDevice {
  // Add a fake identity to the device, but keep the user signed out.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedOut];

  // Simulate opening the URL from an external app.
  [ChromeEarlGrey
      simulateExternalAppURLOpeningWithURL:GetDeepLinkURLForEmail(
                                               fakeIdentity.userEmail)];

  CheckAccountSignin(fakeIdentity, 1,
                     CrossDeviceInitialState::kSignedOutTargetAccountOnDevice);
}

// Tests that opening a cross-device sign-in deep link when sign-in is disabled
// by enterprise policy doesn't trigger the sign-in UI.
- (void)testCrossDeviceSigninDisabledByPolicy {
  // Disable sign-in with policy.
  policy_test_utils::SetPolicy(static_cast<int>(BrowserSigninMode::kDisabled),
                               policy::key::kBrowserSignin);

  // Add a fake identity to the device, but keep the user signed out.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Simulate opening the URL from an external app.
  [ChromeEarlGrey
      simulateExternalAppURLOpeningWithURL:GetDeepLinkURLForEmail(
                                               fakeIdentity.userEmail)];

  // Verify that the sign-in screen is not presented.
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SigninScreenPromoMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that opening a cross-device sign-in deep link when sign-in is disabled
// manually by the user does not show the sign-in screen or any prompt.
- (void)testCrossDeviceSigninDisabledByUser {
  // Disable sign-in manually.
  [ChromeEarlGrey setBoolValue:NO
             forLocalStatePref:prefs::kSigninAllowedOnDevice];

  // Add a fake identity to the device, but keep the user signed out.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Simulate opening the URL from an external app.
  [ChromeEarlGrey
      simulateExternalAppURLOpeningWithURL:GetDeepLinkURLForEmail(
                                               fakeIdentity.userEmail)];

  // Verify that the sign-in screen is not presented.
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SigninScreenPromoMatcher()]
      assertWithMatcher:grey_nil()];
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(CrossDeviceInitialState::
                                                           kFlowForbidden)
                         forHistogram:@"Signin.CrossDevice.InitialState"],
      @"Failed to record InitialState");
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(CrossDeviceInitialState::
                                                           kFlowForbidden)
                         forHistogram:
                             @"Signin.CrossDevice.InitialState.DesktopDefault"],
      @"Failed to record InitialState");
}

// Tests that opening a cross-device sign-in deep link when no accounts on
// device shows the add account flow.
- (void)testCrossDeviceSigninNoAccountOnDevice {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  // Simulate opening the URL from an external app.
  [ChromeEarlGrey
      simulateExternalAppURLOpeningWithURL:GetDeepLinkURLForEmail(
                                               fakeIdentity.userEmail)];

  // Verify that the "Add Account" flow is shown.
  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(kFakeAuthAddAccountButtonIdentifier)];

  // Finish add account flow by adding `fakeIdentity`.
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthAddAccountButtonIdentifier)]
      performAction:grey_tap()];

  CheckAccountSignin(
      fakeIdentity, 0,
      CrossDeviceInitialState::kSignedOutTargetAccountNotOnDevice);
}

// Tests that opening a cross-device sign-in deep link with an account not on
// device while there is already another account on device shows add account
// flow.
- (void)testCrossDeviceSigninSignedOutTargetAccountNotOnDevice {
  // Add a fake identity to the device.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];

  // Simulate opening the URL from an external app with another email.
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [ChromeEarlGrey
      simulateExternalAppURLOpeningWithURL:GetDeepLinkURLForEmail(
                                               fakeIdentity2.userEmail)];

  // Verify that the "Add Account" flow is shown.
  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(kFakeAuthAddAccountButtonIdentifier)];

  // Finish add account flow by adding `fakeIdentity2`.
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthAddAccountButtonIdentifier)]
      performAction:grey_tap()];

  CheckAccountSignin(
      fakeIdentity2, 1,
      CrossDeviceInitialState::kSignedOutTargetAccountNotOnDevice);
}

// Tests that opening a cross-device sign-in deep link with an account not on
// device while there is already another account signed-in shows add account
// flow.
- (void)testCrossDeviceSigninSignedInTargetAccountNotOnDevice {
  // Sign in with `fakeIdentity1`.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];

  // Simulate opening the URL from an external app with another email.
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [ChromeEarlGrey
      simulateExternalAppURLOpeningWithURL:GetDeepLinkURLForEmail(
                                               fakeIdentity2.userEmail)];

  // Verify that the "Add Account" flow is shown.
  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(kFakeAuthAddAccountButtonIdentifier)];

  // Finish add account flow by adding `fakeIdentity2`.
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthAddAccountButtonIdentifier)]
      performAction:grey_tap()];

  CheckAccountSwitch(fakeIdentity1, fakeIdentity2, 1,
                     CrossDeviceInitialState::
                         kSignedInWithDifferentAccountTargetAccountNotOnDevice);
}

// Tests that opening a cross-device sign-in deep link with an account on device
// while there is already another account signed-in shows switch account flow.
- (void)testCrossDeviceSigninSignedInTargetAccountOnDevice {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Sign in with `fakeIdentity1`.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];

  // Simulate opening the URL from an external app with `fakeIdentity2` email.
  [ChromeEarlGrey
      simulateExternalAppURLOpeningWithURL:GetDeepLinkURLForEmail(
                                               fakeIdentity2.userEmail)];

  CheckAccountSwitch(fakeIdentity1, fakeIdentity2, 2,
                     CrossDeviceInitialState::
                         kSignedInWithDifferentAccountTargetAccountOnDevice);
}

// Tests that users changing target account to signed-in account in fullscreen
// signin screen view dismisses the flow. Regression test for:
// crbug.com/537715404.
- (void)testCrossDeviceSigninChangeToSignedInAccount {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Sign in with `fakeIdentity1`.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];

  // Simulate opening the URL from an external app with `fakeIdentity2` email.
  [ChromeEarlGrey
      simulateExternalAppURLOpeningWithURL:GetDeepLinkURLForEmail(
                                               fakeIdentity2.userEmail)];
  [ChromeEarlGrey waitForMatcher:chrome_test_util::SigninScreenPromoMatcher()];

  // Open the identity chooser and sign-in with `fakeIdentity1`.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity1.userEmail)]
      performAction:grey_tap()];
  id<GREYMatcher> primaryButton =
      FullscreenSigninPrimaryButtonMatcher(fakeIdentity1);
  [[EarlGrey selectElementWithMatcher:primaryButton] performAction:grey_tap()];

  // Verify that `fakeIdentity1` is still signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];
}

// Tests that opening a cross-device sign-in deep link with an unknown entry
// point.
- (void)testCrossDeviceSigninUnknownEntryPoint {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  NSString* urlString =
      [NSString stringWithFormat:@"%s?email=%@&entry_point_id=42",
                                 kCrossDeviceSigninUrl, fakeIdentity.userEmail];
  [ChromeEarlGrey
      simulateExternalAppURLOpeningWithURL:[NSURL URLWithString:urlString]];

  [ChromeEarlGrey waitForMatcher:chrome_test_util::SigninScreenPromoMatcher()];
  CheckMetrics(signin::ExternalEntryPoint::kUnknown, 1,
               CrossDeviceInitialState::kSignedOutTargetAccountOnDevice);
}

// Tests that opening a cross-device sign-in deep link and cancelling the "Add
// Account" flow does not show the sign-in UI.
- (void)testCrossDeviceSigninCancelAddAccount {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  // Simulate opening the URL from an external app.
  [ChromeEarlGrey
      simulateExternalAppURLOpeningWithURL:GetDeepLinkURLForEmail(
                                               fakeIdentity.userEmail)];

  // Verify that the "Add Account" flow is shown.
  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(kFakeAuthAddAccountButtonIdentifier)];

  // Cancel the add account flow.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthCancelButtonIdentifier)]
      performAction:grey_tap()];

  // Verify that the sign-in screen is not presented.
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SigninScreenPromoMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that opening a cross-device sign-in deep link with a managed account
// shows the management notice.
- (void)testCrossDeviceSigninEnterprise {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Simulate opening the URL from an external app.
  [ChromeEarlGrey
      simulateExternalAppURLOpeningWithURL:GetDeepLinkURLForEmail(
                                               fakeIdentity.userEmail)];

  [ChromeEarlGrey waitForMatcher:chrome_test_util::SigninScreenPromoMatcher()];

  id<GREYMatcher> primaryButton =
      FullscreenSigninPrimaryButtonMatcher(fakeIdentity);
  [[EarlGrey selectElementWithMatcher:primaryButton] performAction:grey_tap()];

  // Verify that the management notice is shown.
  [ChromeEarlGrey
      waitForMatcher:StaticTextWithAccessibilityLabelId(
                         IDS_IOS_ENTERPRISE_PROFILE_CREATION_TITLE)];
}

// Tests that visiting the cross-device sign-in URL without any parameters does
// not trigger the sign-in UI.
- (void)testCrossDeviceSigninNoParameters {
  NSURL* url = [NSURL
      URLWithString:[NSString stringWithUTF8String:kCrossDeviceSigninUrl]];
  [ChromeEarlGrey simulateExternalAppURLOpeningWithURL:url];

  // Verify that the sign-in screen is not presented.
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SigninScreenPromoMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that opening a cross-device sign-in deep link in incognito mode
// does not trigger the sign-in UI.
- (void)testCrossDeviceSigninInIncognito {
  // Add a fake identity to the device, but keep the user signed out.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  // Load the URL in the incognito tab.
  NSURL* url = GetDeepLinkURLForEmail(fakeIdentity.userEmail);
  [ChromeEarlGrey loadURL:GURL(base::SysNSStringToUTF8(url.absoluteString))
        waitForCompletion:NO];

  // Dismiss the dialog warning that url will be opened in another app after
  // exiting incognito mode.
  id<GREYMatcher> dialogMatcher = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_OPEN_ANOTHER_APP_FROM_INCOGNITO));
  [ChromeEarlGrey waitForMatcher:dialogMatcher];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_OPEN_ANOTHER_APP_BLOCK)]
      performAction:grey_tap()];

  // Verify that the sign-in screen is not presented.
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SigninScreenPromoMatcher()]
      assertWithMatcher:grey_nil()];
}

@end

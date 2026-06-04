// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/webauthn/ios/features.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/webauthn/test/ios_chrome_passkey_client_app_interface.h"
#import "ios/chrome/browser/webauthn/ui/passkey_incognito_interstitial_view_controller.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Returns the matcher for the "Create" button.
id<GREYMatcher> CreatePasskeyButton() {
  return chrome_test_util::StaticTextWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_CREATE));
}

// Returns the matcher for the incognito interstitial's "Continue" button.
id<GREYMatcher> IncognitoContinueButton() {
  return grey_accessibilityID(kButtonStackPrimaryActionAccessibilityIdentifier);
}

// Returns the matcher for the incognito interstitial's "Cancel" button.
id<GREYMatcher> IncognitoCancelButton() {
  return grey_accessibilityID(
      kButtonStackSecondaryActionAccessibilityIdentifier);
}

// Replace the matcher for the incognito interstitial bottom sheet.
id<GREYMatcher> IncognitoInterstitialView() {
  return grey_accessibilityID(kPasskeyIncognitoInterstitialViewID);
}

}  // namespace

@interface PasskeyEGTest : ChromeTestCase

@end

@implementation PasskeyEGTest

- (void)setUp {
  [super setUp];

  // Make sure the fake passkey keychain provider bridge is set.
  [IOSChromePasskeyClientAppInterface setUpFakePasskeyKeychainProviderBridge];

  // Mock a successful reauthentication result by default.
  [IOSChromePasskeyClientAppInterface
      setMockReauthenticationResult:ReauthenticationResult::kSuccess];

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Sign in.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  config.features_enabled.push_back(kIOSPasskeyModalLoginWithShim);

  return config;
}

#pragma mark - Helper methods

- (void)loadPasskeyCreationPage {
  GURL pageURL = self.testServer->GetURL("localhost",
                                         "/navigator_credentials_create.html");
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"Credential Create Test Page"];
  [ChromeEarlGrey tapWebStateElementWithID:@"create-passkey-btn"];
}

- (void)loadPasskeyCancelPage {
  GURL pageURL = self.testServer->GetURL("localhost",
                                         "/navigator_credentials_cancel.html");
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"Credential Cancel Test Page"];
  [ChromeEarlGrey tapWebStateElementWithID:@"create-passkey-btn"];
}

#pragma mark - Tests

- (void)testModalPasskeyCreationInfobar {
  [self loadPasskeyCreationPage];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CreatePasskeyButton()];

  [[EarlGrey selectElementWithMatcher:CreatePasskeyButton()]
      performAction:grey_tap()];

  std::u16string infobarTitleText =
      l10n_util::GetStringUTF16(IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_SAVED);
  [ChromeEarlGrey
      waitForMatcher:grey_text(base::SysUTF16ToNSString(infobarTitleText))];
}

// Tests that the dialog box is visible when triggered in Incognito.
- (void)testIncognitoInterstitialIsVisible {
  [ChromeEarlGrey openNewIncognitoTab];
  [self loadPasskeyCreationPage];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:IncognitoInterstitialView()];
}

// Tests that tapping the "Cancel" button dismisses the sheet and aborts the
// flow.
- (void)testIncognitoInterstitialCancelDismissesSheet {
  [ChromeEarlGrey openNewIncognitoTab];
  [self loadPasskeyCreationPage];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:IncognitoCancelButton()];

  [[EarlGrey selectElementWithMatcher:IncognitoCancelButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:IncognitoInterstitialView()];

  [[EarlGrey selectElementWithMatcher:CreatePasskeyButton()]
      assertWithMatcher:grey_nil()];
}

// Tests that tapping the "Continue" button dismisses the sheet and resumes the
// creation flow.
- (void)testIncognitoInterstitialContinueShowsCreationSheet {
  [ChromeEarlGrey openNewIncognitoTab];
  [self loadPasskeyCreationPage];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:IncognitoContinueButton()];

  [[EarlGrey selectElementWithMatcher:IncognitoContinueButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:IncognitoInterstitialView()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CreatePasskeyButton()];
}

// Tests that the passkey creation bottom sheet is automatically dismissed when
// the webpage fires an AbortSignal.
- (void)testAbortSignalDismissesCreationSheet {
  [self loadPasskeyCancelPage];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:CreatePasskeyButton()];

  // Explicitly trigger the abort signal.
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:@"window.triggerAbort()"];

  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:CreatePasskeyButton()];
}

// Tests that the incognito interstitial is automatically dismissed when the
// webpage fires an AbortSignal.
- (void)testAbortSignalDismissesIncognitoInterstitial {
  [ChromeEarlGrey openNewIncognitoTab];

  [self loadPasskeyCancelPage];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:IncognitoInterstitialView()];

  // Explicitly trigger the abort signal.
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:@"window.triggerAbort()"];

  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:IncognitoInterstitialView()];
}

@end

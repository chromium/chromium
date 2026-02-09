// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/webauthn/ios/features.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/webauthn/test/ios_chrome_passkey_client_app_interface.h"
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

namespace {

// Returns the matcher for the "Create" button.
id<GREYMatcher> CreatePasskeyButton() {
  return chrome_test_util::StaticTextWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_PASSKEY_CREATION_BOTTOM_SHEET_CREATE));
}

}  // namespace

@interface PasskeyEGTest : ChromeTestCase

@end

@implementation PasskeyEGTest

- (void)setUp {
  [super setUp];

  // Make sure the fake passkey keychain provider bridge is set.
  [IOSChromePasskeyClientAppInterface setUpFakePasskeyKeychainProviderBridge];

  // Make sure the mock reauthentication module is set and will return success.
  [IOSChromePasskeyClientAppInterface setUpMockReauthenticationModule];

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Sign in.
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
}

- (void)tearDownHelper {
  [IOSChromePasskeyClientAppInterface removeMockReauthenticationModule];
  [super tearDownHelper];
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

@end

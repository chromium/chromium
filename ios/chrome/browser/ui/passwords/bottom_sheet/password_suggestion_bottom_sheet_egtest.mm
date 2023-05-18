// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/passwords/password_manager_app_interface.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/base/mac/url_conversions.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

static constexpr char kFormPassword[] = "pw";

namespace {

using base::test::ios::kWaitForActionTimeout;

BOOL WaitForKeyboardToAppear() {
  GREYCondition* waitForKeyboard = [GREYCondition
      conditionWithName:@"Wait for keyboard"
                  block:^BOOL {
                    return [EarlGrey isKeyboardShownWithError:nil];
                  }];
  return [waitForKeyboard waitWithTimeout:kWaitForActionTimeout.InSecondsF()];
}

}  // namespace

@interface PasswordSuggestionBottomSheetEGTest : ChromeTestCase
@end

@implementation PasswordSuggestionBottomSheetEGTest

- (void)setUp {
  [super setUp];

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");

  // Prefs aren't reset between tests, crbug.com/1069086. Most tests don't care
  // about the account storage notice, so suppress it by marking it as shown.
  [PasswordManagerAppInterface setAccountStorageNoticeShown:YES];
  // Also reset the dismiss count pref to 0 to make sure the bottom sheet is
  // enabled by default.
  [PasswordSuggestionBottomSheetAppInterface setDismissCount:0];
}

- (void)tearDown {
  [PasswordManagerAppInterface clearCredentials];
  [PasswordSuggestionBottomSheetAppInterface removeMockReauthenticationModule];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      password_manager::features::kIOSPasswordBottomSheet);
  return config;
}

#pragma mark - Helper methods

// Loads simple page on localhost.
- (void)loadLoginPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_login_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];
}

#pragma mark - Tests

- (void)testOpenPasswordBottomSheetUsePassword {
  [PasswordSuggestionBottomSheetAppInterface setUpMockReauthenticationModule];
  [PasswordSuggestionBottomSheetAppInterface
      mockReauthenticationModuleExpectedResult:ReauthenticationResult::
                                                   kSuccess];
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form.html"))];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD))]
      performAction:grey_tap()];
}

- (void)testOpenPasswordBottomSheetTapNoThanksShowKeyboard {
  [PasswordManagerAppInterface
      storeCredentialWithUsername:@"user"
                         password:@"password"
                              URL:net::NSURLWithGURL(self.testServer->GetURL(
                                      "/simple_login_form.html"))];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  [self loadLoginPage];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(@"user")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_PASSWORD_BOTTOM_SHEET_NO_THANKS))]
      performAction:grey_tap()];

  WaitForKeyboardToAppear();
}

@end

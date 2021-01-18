// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include <memory>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/passwords/password_manager_app_interface.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
// TODO(crbug.com/1015113): The EG2 macro is breaking indexing for some reason
// without the trailing semicolon.  For now, disable the extra semi warning
// so Xcode indexing works for the egtest.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(PasswordManagerAppInterface);
#endif  // defined(CHROME_EARL_GREY_2)

constexpr char kFormUsername[] = "un";
constexpr char kFormPassword[] = "pw";
NSString* const kSavedCredentialLabel = @"Eguser, Hidden, Password";

namespace {

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForActionTimeout;

id<GREYMatcher> PasswordInfobar(int prompt_id) {
  NSString* bannerLabel =
      [NSString stringWithFormat:@"%@, %@", l10n_util::GetNSString(prompt_id),
                                 kSavedCredentialLabel];
  return grey_allOf(grey_accessibilityID(kInfobarBannerViewIdentifier),
                    grey_accessibilityLabel(bannerLabel), nil);
}

id<GREYMatcher> PasswordInfobarButton(int button_id) {
  return chrome_test_util::ButtonWithAccessibilityLabelId(button_id);
}

}  // namespace

@interface PasswordControllerEGTest : WebHttpServerChromeTestCase
@end

@implementation PasswordControllerEGTest

- (void)setUp {
  [super setUp];

  // Set up server.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (void)tearDown {
  [PasswordManagerAppInterface clearCredentials];
  [super tearDown];
}

#pragma mark - Helper methods

// Loads simple page on localhost.
- (void)loadLoginPage {
  // Loads simple page. It is on localhost so it is considered a secure context.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/simple_login_form.html")];
  [ChromeEarlGrey waitForWebStateContainingText:"Login form."];
}

#pragma mark - Tests

// Tests that save password prompt is shown on new login.
- (void)testSavePromptAppearsOnFormSubmission {
  [self loadLoginPage];

  // Simulate user interacting with fields.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormUsername)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("submit_button")];

  // Wait until the save password prompt becomes visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          PasswordInfobar(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT)];

  [[EarlGrey selectElementWithMatcher:PasswordInfobarButton(
                                          IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON)]
      performAction:grey_tap()];

  // Wait until the save password infobar disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          PasswordInfobar(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT)];

  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"Wrong number of stored credentials.");
}

// Tests that update password prompt is shown on submitting the new password
// for an already stored login.
- (void)testUpdatePromptAppearsOnFormSubmission {
  // Load the page the first time an store credentials.
  [self loadLoginPage];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"Eguser"
                                                  password:@"OldPass"];
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"Wrong number of initial credentials.");

  // Load the page again and have a new password value to save.
  [self loadLoginPage];
  // Simulate user interacting with fields.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormUsername)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormPassword)];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId("submit_button")];

  // Wait until the update password prompt becomes visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          PasswordInfobar(IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD)];

  [[EarlGrey
      selectElementWithMatcher:PasswordInfobarButton(
                                   IDS_IOS_PASSWORD_MANAGER_UPDATE_BUTTON)]
      performAction:grey_tap()];

  // Wait until the update password infobar disappears.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          PasswordInfobar(IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD)];

  credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"Wrong number of final credentials.");
}

@end

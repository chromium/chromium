// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include <memory>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/passwords/password_manager_app_interface.h"
#include "ios/chrome/browser/passwords/password_manager_features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

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

namespace {

// Notification should be displayed for 3 seconds. 4 should be safe to check.
constexpr CFTimeInterval kDisappearanceTimeout = 4;
// Provides basic response for example page.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(
      "<head><title>Example website</title></head>"
      "<body>You are here.</body>");
  return std::move(http_response);
}

}  // namespace

// This class tests UI behavior for Credential Manager.
// TODO(crbug.com/435048): Add EG test for save/update password prompt.
// TODO(crbug.com/435048): When account chooser and first run experience dialog
// are implemented, test them too.
@interface CredentialManagerEGTest : ChromeTestCase

@end

@implementation CredentialManagerEGTest {
  base::test::ScopedFeatureList _featureList;
}

- (void)setUp {
  _featureList.InitAndEnableFeature(features::kCredentialManager);

  [super setUp];

  // Set up server.
  self.testServer->RegisterRequestHandler(base::Bind(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)tearDown {
  [PasswordManagerAppInterface clearCredentials];
  [super tearDown];
}

- (void)launchAppForTestMethod {
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithFeaturesEnabled:{features::kCredentialManager}
                                  disabled:{}
                              forceRestart:NO];
}

#pragma mark - Utils

// Loads simple page on localhost and stores an example PasswordCredential.
- (void)loadSimplePageAndStoreACredential {
  // Loads simple page. It is on localhost so it is considered a secure context.
  const GURL URL = self.testServer->GetURL("/example");
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"You are here."];

  NSError* error = [PasswordManagerAppInterface
      storeCredentialWithUsername:@"johndoe@example.com"
                         password:@"ilovejanedoe123"];
  GREYAssertNil(error, error.localizedDescription);
}

#pragma mark - Tests

// Tests that notification saying "Signing is as ..." appears on auto sign-in.
- (void)testNotificationAppearsOnAutoSignIn {
  // TODO(crbug.com/786960): re-enable when fixed. Tests may pass on EG2
#if defined(CHROME_EARL_GREY_1)
  EARL_GREY_TEST_DISABLED(@"Fails on iOS 11.0.");
#endif

  [PasswordManagerAppInterface setAutosigninPreferences];
  [self loadSimplePageAndStoreACredential];

  // Call get() from JavaScript.
  NSError* error = nil;
  NSString* result = [ChromeEarlGreyAppInterface
      executeJavaScript:@"typeof navigator.credentials.get({password: true})"
                  error:&error];
  GREYAssertTrue([result isEqual:@"object"],
                 @"Unexpected error occurred when executing JavaScript.");
  GREYAssertTrue(!error,
                 @"Unexpected error occurred when executing JavaScript.");

  // Matches the UILabel by its accessibilityLabel.
  id<GREYMatcher> matcher =
      grey_allOf(grey_accessibilityLabel(@"Signing in as johndoe@example.com"),
                 grey_accessibilityTrait(UIAccessibilityTraitStaticText), nil);
  // Wait for notification to appear.
  ConditionBlock waitForAppearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()
                                                             error:&error];
    return error == nil;
  };
  // Gives some time for the notification to appear.
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, waitForAppearance),
             @"Notification did not appear");
  // Wait for the notification to disappear.
  ConditionBlock waitForDisappearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  // Ensures that notification disappears after time limit.
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(kDisappearanceTimeout,
                                                          waitForDisappearance),
             @"Notification did not disappear");
}

// Tests that when navigator.credentials.get() was called from inactive tab, the
// autosign-in notification appears once tab becomes active.
- (void)testNotificationAppearsWhenTabIsActive {
  // TODO(crbug.com/786960): re-enable when fixed. Tests may pass on EG2
#if defined(CHROME_EARL_GREY_1)
  EARL_GREY_TEST_DISABLED(@"Fails on iOS 11.0.");
#endif
  [PasswordManagerAppInterface setAutosigninPreferences];
  [self loadSimplePageAndStoreACredential];

  // Open new tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey waitForMainTabCount:2];

  [PasswordManagerAppInterface getCredentialsInTabAtIndex:0];

  // Matches the UILabel by its accessibilityLabel.
  id<GREYMatcher> matcher = chrome_test_util::StaticTextWithAccessibilityLabel(
      @"Signing in as johndoe@example.com");
  // Wait for notification to appear.
  ConditionBlock waitForAppearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };

  // Check that notification doesn't appear in current tab.
  GREYAssertFalse(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout, waitForAppearance),
      @"Notification appeared in wrong tab");

  // Switch to previous tab.
  [ChromeEarlGrey selectTabAtIndex:0];

  // Check that the notification has appeared.
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, waitForAppearance),
             @"Notification did not appear");
}

@end

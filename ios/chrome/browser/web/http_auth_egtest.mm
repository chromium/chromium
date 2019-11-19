// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_auth_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace {

// Returns matcher for HTTP Authentication dialog.
id<GREYMatcher> HttpAuthDialog() {
  NSString* title = l10n_util::GetNSStringWithFixup(IDS_LOGIN_DIALOG_TITLE);
  return chrome_test_util::StaticTextWithAccessibilityLabel(title);
}

// Returns matcher for Username text field.
id<GREYMatcher> UsernameField() {
  if (@available(iOS 13.0, *)) {
    return grey_accessibilityValue(l10n_util::GetNSStringWithFixup(
        IDS_IOS_HTTP_LOGIN_DIALOG_USERNAME_PLACEHOLDER));
  } else {
    return chrome_test_util::StaticTextWithAccessibilityLabelId(
        IDS_IOS_HTTP_LOGIN_DIALOG_USERNAME_PLACEHOLDER);
  }
}

// Returns matcher for Password text field.
id<GREYMatcher> PasswordField() {
  if (@available(iOS 13.0, *)) {
    return grey_accessibilityValue(l10n_util::GetNSStringWithFixup(
        IDS_IOS_HTTP_LOGIN_DIALOG_PASSWORD_PLACEHOLDER));
  } else {
    return chrome_test_util::StaticTextWithAccessibilityLabelId(
        IDS_IOS_HTTP_LOGIN_DIALOG_PASSWORD_PLACEHOLDER);
  }
}

// Returns matcher for Login button.
id<GREYMatcher> LoginButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_LOGIN_DIALOG_OK_BUTTON_LABEL);
}

// Waits until static text with IDS_LOGIN_DIALOG_TITLE label is displayed.
void WaitForHttpAuthDialog() {
  BOOL dialog_shown = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:HttpAuthDialog()]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return !error;
  });
  GREYAssert(dialog_shown, @"HTTP Authentication dialog was not shown");
}

}  // namespace

// Test case for HTTP Authentication flow.
@interface HTTPAuthTestCase : ChromeTestCase
@end

@implementation HTTPAuthTestCase

// Tests Basic HTTP Authentication with correct username and password.
- (void)testSuccessfullBasicAuth {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // EG does not allow interactions with HTTP Dialog when loading spinner is
    // animated. TODO(crbug.com/680290): Enable this test on iPad when EarlGrey
    // allows tapping dialog buttons with active page load spinner.
    EARL_GREY_TEST_DISABLED(@"Tab Title not displayed on handset.");
  }

  GURL URL = web::test::HttpServer::MakeUrl("http://good-auth");
  web::test::SetUpHttpServer(std::make_unique<web::HttpAuthResponseProvider>(
      URL, "GoodRealm", "gooduser", "goodpass"));
  [ChromeEarlGrey loadURL:URL waitForCompletion:NO];
  WaitForHttpAuthDialog();

  // Enter valid username and password.
  [[EarlGrey selectElementWithMatcher:UsernameField()]
      performAction:grey_typeText(@"gooduser")];
  [[EarlGrey selectElementWithMatcher:PasswordField()]
      performAction:grey_typeText(@"goodpass")];
  [[EarlGrey selectElementWithMatcher:LoginButton()] performAction:grey_tap()];

  const std::string pageText = web::HttpAuthResponseProvider::page_text();
  [ChromeEarlGrey waitForWebStateContainingText:pageText];
}

// Tests Basic HTTP Authentication with incorrect username and password.
- (void)testUnsuccessfullBasicAuth {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // EG does not allow interactions with HTTP Dialog when loading spinner is
    // animated. TODO(crbug.com/680290): Enable this test on iPad when EarlGrey
    // allows tapping dialog buttons with active page load spinner.
    EARL_GREY_TEST_DISABLED(@"Tab Title not displayed on handset.");
  }

  GURL URL = web::test::HttpServer::MakeUrl("http://bad-auth");
  web::test::SetUpHttpServer(std::make_unique<web::HttpAuthResponseProvider>(
      URL, "BadRealm", "baduser", "badpass"));
  [ChromeEarlGrey loadURL:URL waitForCompletion:NO];
  WaitForHttpAuthDialog();

  // Enter invalid username and password.
  [[EarlGrey selectElementWithMatcher:UsernameField()]
      performAction:grey_typeText(@"gooduser")];
  [[EarlGrey selectElementWithMatcher:PasswordField()]
      performAction:grey_typeText(@"goodpass")];
  [[EarlGrey selectElementWithMatcher:LoginButton()] performAction:grey_tap()];

  // Verifies that authentication was requested again.
  WaitForHttpAuthDialog();
}

// Tests Cancelling Basic HTTP Authentication.
- (void)testCancellingBasicAuth {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // EG does not allow interactions with HTTP Dialog when loading spinner is
    // animated. TODO(crbug.com/680290): Enable this test on iPad when EarlGrey
    // allows tapping dialog buttons with active page load spinner.
    EARL_GREY_TEST_DISABLED(@"Tab Title not displayed on handset.");
  }

  GURL URL = web::test::HttpServer::MakeUrl("http://cancel-auth");
  web::test::SetUpHttpServer(std::make_unique<web::HttpAuthResponseProvider>(
      URL, "CancellingRealm", "", ""));
  [ChromeEarlGrey loadURL:URL waitForCompletion:NO];
  WaitForHttpAuthDialog();

  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:HttpAuthDialog()]
      assertWithMatcher:grey_nil()];
}

@end

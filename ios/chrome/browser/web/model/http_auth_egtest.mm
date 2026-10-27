// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/base64.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/threading/platform_thread.h"
#import "base/time/time.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Returns matcher for the HTTP Authentication dialog title label. Labels inside
// buttons are excluded because the dialog's OK button and the NTP identity disc
// button also contain a "Sign in" label.
id<GREYMatcher> HttpAuthDialog() {
  NSString* title = l10n_util::GetNSStringWithFixup(IDS_LOGIN_DIALOG_TITLE);
  return grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(title),
                    grey_not(grey_ancestor(grey_kindOfClass(UIButton.class))),
                    nil);
}

// Returns matcher for Username text field.
id<GREYMatcher> UsernameField() {
  return grey_accessibilityValue(l10n_util::GetNSStringWithFixup(
      IDS_IOS_HTTP_LOGIN_DIALOG_USERNAME_PLACEHOLDER));
}

// Returns matcher for Password text field.
id<GREYMatcher> PasswordField() {
  return grey_accessibilityValue(l10n_util::GetNSStringWithFixup(
      IDS_IOS_HTTP_LOGIN_DIALOG_PASSWORD_PLACEHOLDER));
}

// Returns matcher for the Login button once it is enabled. Alert buttons are
// initially disabled, see crbug.com/341353783.
id<GREYMatcher> EnabledLoginButton() {
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_LOGIN_DIALOG_OK_BUTTON_LABEL),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

// Returns matcher for the Cancel button once it is enabled. Alert buttons are
// initially disabled, see crbug.com/341353783.
id<GREYMatcher> EnabledCancelButton() {
  return grey_allOf(
      chrome_test_util::CancelButton(),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
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

const char kAuthPageText[] = "authenticated";

// Returns a response with the given `code` and `realm`.
std::unique_ptr<net::test_server::HttpResponse> GetAuthResponse(
    net::HttpStatusCode code,
    const std::string& realm) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(code);
  if (code == net::HTTP_UNAUTHORIZED) {
    response->AddCustomHeader("WWW-Authenticate",
                              "Basic realm=\"" + realm + "\"");
  } else {
    response->set_content(kAuthPageText);
  }
  return response;
}

// Request handler for authentication.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/good-auth") {
    auto it = request.headers.find("Authorization");
    std::string expected_auth =
        "Basic " + base::Base64Encode("gooduser:goodpass");
    if (it != request.headers.end() && it->second == expected_auth) {
      return GetAuthResponse(net::HTTP_OK, "");
    }
    return GetAuthResponse(net::HTTP_UNAUTHORIZED, "GoodRealm");
  }
  if (request.relative_url == "/bad-auth") {
    // This will always request authentication.
    return GetAuthResponse(net::HTTP_UNAUTHORIZED, "BadRealm");
  }
  if (request.relative_url == "/cancel-auth") {
    return GetAuthResponse(net::HTTP_UNAUTHORIZED, "CancellingRealm");
  }
  return nullptr;
}

}  // namespace

// Test case for HTTP Authentication flow.
@interface HTTPAuthTestCase : ChromeTestCase
@end

@implementation HTTPAuthTestCase

- (void)setUp {
  [super setUp];
  // Each test relaunches the app, then starts the HTTP authentication load
  // without waiting for the startup New Tab Page load to finish. Wait for it
  // first, otherwise the authentication dialog is sometimes never shown.
  [ChromeEarlGrey waitForPageToFinishLoading];
  self.testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  GREYAssertTrue(self.testServer->Start(),
                 @"EmbeddedTestServer failed to start.");
}

// Tests Basic HTTP Authentication with correct username and password.
- (void)testSuccessfullBasicAuth {
  // EG synchronization disabled block.
  {
    // EG synchronizes with WKWebView. Disable synchronization for EG interation
    // during when page is loading.
    ScopedSynchronizationDisabler disabler;
    GURL URL = self.testServer->GetURL("/good-auth");
    [ChromeEarlGrey loadURL:URL waitForCompletion:NO];
    WaitForHttpAuthDialog();

    [[EarlGrey selectElementWithMatcher:UsernameField()]
        performAction:grey_tap()];

    // Wait for the keyboard to be shown.
    base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.5));

    // Enter valid username and password.
    [[EarlGrey selectElementWithMatcher:UsernameField()]
        performAction:grey_replaceText(@"gooduser")];
    [[EarlGrey selectElementWithMatcher:PasswordField()]
        performAction:grey_replaceText(@"goodpass")];
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:EnabledLoginButton()];
    [[EarlGrey selectElementWithMatcher:EnabledLoginButton()]
        performAction:grey_tap()];
  }  // EG synchronization disabled block.

  [ChromeEarlGrey waitForWebStateContainingText:kAuthPageText];
}

// Tests Basic HTTP Authentication with incorrect username and password.
- (void)testUnsuccessfullBasicAuth {
  // EG synchronization disabled block.
  {
    // EG synchronizes with WKWebView. Disable synchronization for EG interation
    // during when page is loading.
    ScopedSynchronizationDisabler disabler;
    GURL URL = self.testServer->GetURL("/bad-auth");
    [ChromeEarlGrey loadURL:URL waitForCompletion:NO];
    WaitForHttpAuthDialog();

    // Enter invalid username and password.
    [[EarlGrey selectElementWithMatcher:UsernameField()]
        performAction:grey_replaceText(@"gooduser")];
    [[EarlGrey selectElementWithMatcher:PasswordField()]
        performAction:grey_replaceText(@"goodpass")];
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:EnabledLoginButton()];
    [[EarlGrey selectElementWithMatcher:EnabledLoginButton()]
        performAction:grey_tap()];

    // Ensure first dialog is dismissed before waiting for the second one.
    base::PlatformThread::Sleep(base::Seconds(1));
    // Verifies that authentication was requested again.
    WaitForHttpAuthDialog();
    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:EnabledCancelButton()];
    [[EarlGrey selectElementWithMatcher:EnabledCancelButton()]
        performAction:grey_tap()];
  }  // EG synchronization disabled block.

  [[EarlGrey selectElementWithMatcher:HttpAuthDialog()]
      assertWithMatcher:grey_nil()];
}

// Tests Cancelling Basic HTTP Authentication.
- (void)testCancellingBasicAuth {
  // EG synchronization disabled block.
  {
    // EG synchronizes with WKWebView. Disable synchronization for EG interation
    // during when page is loading.
    ScopedSynchronizationDisabler disabler;
    GURL URL = self.testServer->GetURL("/cancel-auth");
    [ChromeEarlGrey loadURL:URL waitForCompletion:NO];
    WaitForHttpAuthDialog();

    [ChromeEarlGrey waitForUIElementToAppearWithMatcher:EnabledCancelButton()];

    [[EarlGrey selectElementWithMatcher:EnabledCancelButton()]
        performAction:grey_tap()];

  }  // EG synchronization disabled block.

  [[EarlGrey selectElementWithMatcher:HttpAuthDialog()]
      assertWithMatcher:grey_nil()];
}

@end

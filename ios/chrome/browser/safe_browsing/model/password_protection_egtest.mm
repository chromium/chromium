// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <string>

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/passwords/model/password_manager_app_interface.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_constants.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "url/gurl.h"

namespace {

constexpr char kInputPage[] = "Input";
constexpr char kInputElement[] = "input";

id<GREYMatcher> PasswordProtectionMatcher() {
  return grey_accessibilityID(kPasswordProtectionViewAccessibilityIdentifier);
}

// Request handler for net::EmbeddedTestServer that serves a simple input
// textfield.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content("Input: <input type='text' id='input'>");
  http_response->set_content_type("text/html");
  return http_response;
}
}  // namespace

// Tests PhishGuard saved password reuse protection.
@interface PasswordProtectionTestCase : ChromeTestCase {
  // A URL that is treated as an unsafe phishing page by PhishGuard.
  GURL _phishingURL;
  // A URL that is allow listed by PhishGuard.
  GURL _allowlistedURL;
}
@end

@implementation PasswordProtectionTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      password_manager::features::kPasswordReuseDetectionEnabled);
  config.relaunch_policy = NoForceRelaunchAndResetState;

  // Use commandline args to save a fake allowlisted URL.
  config.additional_args.push_back(
      std::string("--mark_as_allowlisted_for_phish_guard=") +
      _allowlistedURL.spec());

  if ([self isRunningTest:@selector(testPasswordReuseDetectionWarning)]) {
    // Use commandline args to save a fake phishing cached verdict.
    config.additional_args.push_back(
        std::string("--mark_as_phish_guard_phishing=") + _phishingURL.spec());
  }

  return config;
}

- (void)setUp {
  self.testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  bool started = self.testServer->Start();
  _phishingURL = self.testServer->GetURL("/phishingURL");
  _allowlistedURL = self.testServer->GetURL("/allowlistedURL");
  [super setUp];
  GREYAssertTrue(started, @"Server did not start.");
  NSURL* URL = [NSURL URLWithString:@"http://www.example.com"];
  [PasswordManagerAppInterface storeCredentialWithUsername:@"Username"
                                                  password:@"Password"
                                                       URL:URL];
  int credentialsCount = [PasswordManagerAppInterface storedCredentialsCount];
  GREYAssertEqual(1, credentialsCount, @"There should be one credential.");
}

- (void)tearDown {
  GREYAssertTrue([PasswordManagerAppInterface clearCredentials],
                 @"Clearing credentials wasn't done.");
  [super tearDown];
}

- (void)typePasswordIntoWebInput {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kInputElement)];

  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"P" flags:UIKeyModifierShift];
  for (NSString* character in @[ @"a", @"s", @"s", @"w", @"o", @"r", @"d" ]) {
    [ChromeEarlGrey simulatePhysicalKeyboardEvent:character flags:0];
  }
}

// Tests that password protection UI is shown when saved password is reused on
// phishing site.
- (void)testPasswordReuseDetectionWarning {
  // PhishGuard is only available on iOS 14.0 or above.

  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:kInputPage];

  [self typePasswordIntoWebInput];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:PasswordProtectionMatcher()
                                  timeout:base::test::ios::
                                              kWaitForUIElementTimeout];
}

// Tests that password protection UI is not shown when saved password is reused
// on safe site.
- (void)testPasswordProtectionNotShownForAllowListedURL {
  // PhishGuard is only available on iOS 14.0 or above.

  [ChromeEarlGrey loadURL:_allowlistedURL];
  [ChromeEarlGrey waitForWebStateContainingText:kInputPage];

  [self typePasswordIntoWebInput];

  [[EarlGrey selectElementWithMatcher:PasswordProtectionMatcher()]
      assertWithMatcher:grey_nil()];
}

@end

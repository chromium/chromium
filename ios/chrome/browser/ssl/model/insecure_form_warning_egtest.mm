// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <string>

#import "base/base_paths.h"
#import "base/functional/bind.h"
#import "base/path_service.h"
#import "base/strings/escape.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#import "components/policy/policy_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_constants.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ssl/model/insecure_form_warning_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

void TapCancel() {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];
}

void TapSubmitAnyway() {
  id<GREYMatcher> button = chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_INSECURE_FORM_SUBMIT_BUTTON);
  [[EarlGrey selectElementWithMatcher:button] performAction:grey_tap()];
}

void WaitForInsecureFormDialog() {
  ConditionBlock condition = ^{
    NSError* error = nil;
    id<GREYMatcher> dialog_matcher =
        grey_accessibilityID(kInsecureFormWarningAccessibilityIdentifier);
    [[EarlGrey selectElementWithMatcher:dialog_matcher]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return !error;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Insecure form warning was not shown.");
}

// Tests submission of insecure forms with POST data.
@interface InsecureFormTestCase : ChromeTestCase {
  // This server serves over HTTPS, but its port is treated as a secure URL.
  std::unique_ptr<net::test_server::EmbeddedTestServer> _fakeHTTPSServer;
}
@end

@implementation InsecureFormTestCase

- (void)setUp {
  [super setUp];
  _fakeHTTPSServer = std::make_unique<net::test_server::EmbeddedTestServer>(
      net::test_server::EmbeddedTestServer::TYPE_HTTP);
  _fakeHTTPSServer->ServeFilesFromDirectory(
      base::PathService::CheckedGet(base::DIR_ASSETS)
          .AppendASCII("ios/testing/data/http_server_files/"));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  GREYAssertTrue(_fakeHTTPSServer->Start(),
                 @"Test HTTPS server failed to start.");

  // Set the HTTP server's URL as insecure. We need this so that the tests don't
  // treat 127.0.0.1:port as secure.
  [InsecureFormWarningAppInterface
      setInsecureFormPortsForTesting:self->_fakeHTTPSServer->port()
               portTreatedAsInsecure:self.testServer->port()];
}

- (void)tearDown {
  [InsecureFormWarningAppInterface setInsecureFormPortsForTesting:0
                                            portTreatedAsInsecure:0];
  policy_test_utils::ClearPolicies();

  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  if ([self isRunningTest:@selector(testInsecureFormSubmit_FeatureDisabled)]) {
    config.features_disabled.push_back(
        security_interstitials::features::kInsecureFormSubmissionInterstitial);
  } else {
    config.features_enabled.push_back(
        security_interstitials::features::kInsecureFormSubmissionInterstitial);
  }
  return config;
}

- (void)submitFormAndExpectNoWarning:(const GURL&)pageURL {
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"Insecure form ready to submit"];

  // Submit the form, should go through without a warning.
  [ChromeEarlGrey tapWebStateElementWithID:@"submit"];
  [ChromeEarlGrey
      waitForWebStateContainingText:"username=testuser&password=testpassword"];
}

#pragma mark - Tests

// Tests that posting from HTTP to HTTP doesn't show a warning.
- (void)testHttpToHttpFormSubmit {
  std::string URLString =
      "/insecure_form.html?" + self.testServer->GetURL("/echo").spec();
  const GURL HTTPToHTTPFormURL = self.testServer->GetURL(URLString);
  [self submitFormAndExpectNoWarning:HTTPToHTTPFormURL];
}

// Tests that posting from HTTPS to HTTP shows a warning and tapping cancel
// doesn't send post data.
- (void)testInsecureFormSubmit_Cancel {
  std::string URLString =
      "/insecure_form.html?" + self.testServer->GetURL("/echo").spec();
  const GURL FakeHTTPSToHTTPFormURL = _fakeHTTPSServer->GetURL(URLString);

  [ChromeEarlGrey loadURL:FakeHTTPSToHTTPFormURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"Insecure form ready to submit"];

  // Submit the form. The insecure form warning should block the
  // submission.
  [ChromeEarlGrey tapWebStateElementWithID:@"submit"];
  WaitForInsecureFormDialog();
  TapCancel();
  [ChromeEarlGrey
      waitForWebStateContainingText:"Insecure form ready to submit"];
}

// Tests that posting from HTTPS to HTTP shows a warning and tapping
// "Send anyway" sends the post data.
- (void)testInsecureFormSubmit_SendAnyway {
  std::string URLString =
      "/insecure_form.html?" + self.testServer->GetURL("/echo").spec();
  const GURL FakeHTTPSToHTTPFormURL = _fakeHTTPSServer->GetURL(URLString);

  [ChromeEarlGrey loadURL:FakeHTTPSToHTTPFormURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"Insecure form ready to submit"];

  // Submit the form. The insecure form warning should block the
  // submission.
  [ChromeEarlGrey tapWebStateElementWithID:@"submit"];
  WaitForInsecureFormDialog();
  TapSubmitAnyway();
  [ChromeEarlGrey
      waitForWebStateContainingText:"username=testuser&password=testpassword"];
}

// Tests that posting from HTTPS to HTTP doesn't show a warning when the feature
// is disabled.
- (void)testInsecureFormSubmit_FeatureDisabled {
  std::string URLString =
      "/insecure_form.html?" + self.testServer->GetURL("/echo").spec();
  const GURL FakeHTTPSToHTTPFormURL = _fakeHTTPSServer->GetURL(URLString);

  [self submitFormAndExpectNoWarning:FakeHTTPSToHTTPFormURL];
}

// Tests that disabling the feature by policy will stop showing a warning.
- (void)testInsecureFormSubmit_DisabledByPolicy {
  policy_test_utils::SetPolicy(false,
                               policy::key::kInsecureFormsWarningsEnabled);

  std::string URLString =
      "/insecure_form.html?" + self.testServer->GetURL("/echo").spec();
  const GURL FakeHTTPSToHTTPFormURL = _fakeHTTPSServer->GetURL(URLString);

  [self submitFormAndExpectNoWarning:FakeHTTPSToHTTPFormURL];
}

@end

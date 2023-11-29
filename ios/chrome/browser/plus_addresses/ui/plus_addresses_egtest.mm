// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/escape.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
const char kEmailFormUrl[] = "/email_signup_form.html";
const char kEmailFieldId[] = "email";
const char kFakeSuggestionLabel[] = "plus?";
}  // namespace

// Test suite that tests plus addresses functionality.
@interface PlusAddressesTestCase : WebHttpServerChromeTestCase
@end

@implementation PlusAddressesTestCase

- (void)setUp {
  [super setUp];
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Ensure the feature is enabled, including a required param.
  // TODO(crbug.com/1467623): Set up fake responses via `self.testServer`, or
  // use an app interface to force different states without a backend
  // dependency.
  config.additional_args.push_back(base::StringPrintf(
      "--enable-features=PlusAddressesEnabled:suggestion-"
      "label/%s/server-url/%s",
      kFakeSuggestionLabel,
      base::EscapeQueryParamValue(self.testServer->base_url().spec(),
                                  /*use_plus=*/false)
          .c_str()));
  return config;
}

#pragma mark - Helper methods

// Loads simple page on localhost, ensuring that it is eligible for the
// plus_addresses feature.
- (void)loadPlusAddressEligiblePage {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kEmailFormUrl)];
  [ChromeEarlGrey waitForWebStateContainingText:"Signup form"];
}

#pragma mark - Tests

// A basic test that simply opens and dismisses the bottom sheet.
- (void)testShowPlusAddressBottomSheet {
  // Force a re-evaluation of the enabled features, since the testServer isn't
  // yet available in the initial `appConfigurationForTestCase` run.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Ensure a fake identity is available, as this is required by the
  // plus_addresses feature.
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  [self loadPlusAddressEligiblePage];

  // Tap an element that is eligible for plus_address autofilling.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kEmailFieldId)];
  id<GREYMatcher> user_chip =
      grey_text(base::SysUTF8ToNSString(kFakeSuggestionLabel));

  // Ensure the plus_address suggestion appears.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:user_chip];

  // Tapping it will trigger the UI.
  // TODO(crbug.com/1467623): Flesh this out as more functionality is
  // implemented. An app interface or demo feature param will be necessary here,
  // too, such that actions that normally trigger server calls can be mocked
  // out.
  [[EarlGrey selectElementWithMatcher:user_chip] performAction:grey_tap()];

  // The request to reserve a plus address is hitting the test server, and
  // should fail immediately.
  id<GREYMatcher> error_message =
      grey_text(l10n_util::GetNSString(IDS_PLUS_ADDRESS_MODAL_ERROR_MESSAGE));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:error_message];

  // Ensure the cancel button is shown.
  id<GREYMatcher> cancelButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT);

  // Click the cancel button, dismissing the bottom sheet.
  [[EarlGrey selectElementWithMatcher:cancelButton] performAction:grey_tap()];
}

@end

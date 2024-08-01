// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_test_case.h"

#import "base/strings/strcat.h"
#import "base/test/ios/wait_util.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/policy_switches.h"
#import "components/policy/test_support/embedded_policy_test_server.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/http/http_status_code.h"

namespace {

constexpr char kEnrollmentToken[] = "enrollment_token";

}  // namespace

// Test the enterprise loading screen.
@interface EnterpriseLoadingScreenTestCase : ChromeTestCase {
  // The fake implementation of the cloud policy server.
  std::unique_ptr<policy::EmbeddedPolicyTestServer> _policyTestServer;
}

@end

@implementation EnterpriseLoadingScreenTestCase

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];

  _policyTestServer = std::make_unique<policy::EmbeddedPolicyTestServer>();
  GREYAssertTrue(_policyTestServer->Start(),
                 @"Policy test server did not start.");

  [self clearPolicies];
}

- (void)tearDown {
  [self clearPolicies];
  [super tearDown];
}

#pragma mark - Helpers

// Removes all policies stored in memory.
- (void)clearPolicies {
  [PolicyAppInterface clearAllPoliciesInNSUserDefault];
  GREYAssertTrue([PolicyAppInterface clearDMTokenDirectory],
                 @"DM token directory not cleared.");
  GREYAssertTrue([PolicyAppInterface clearCloudPolicyDirectory],
                 @"Cloud policy directory not cleared.");
}

// Waits and verify that the enterprise loading screen is not displayed.
- (void)verifyLoadingScreenIsDismissed {
  GREYCondition* loadingScreenDismissed = [GREYCondition
      conditionWithName:@"Wait for enterprise loading screen to be dismissed"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey
                        selectElementWithMatcher:
                            grey_accessibilityID(
                                first_run::
                                    kEnterpriseLoadingScreenAccessibilityIdentifier)]
                        assertWithMatcher:grey_notVisible()
                                    error:&error];
                    return error == nil;
                  }];
  // Waits for enterprise loading screen to be dismissed or timeout after
  // kWaitForUIElementTimeout.
  GREYAssertTrue([loadingScreenDismissed
                     waitWithTimeout:base::test::ios::kWaitForUIElementTimeout
                                         .InSecondsF()],
                 @"Enterprise loading screen still visible.");
}

// Configures the app to have all conditions needed to display the enterprise
// loading screen. If `enrollmentTokenValid` is set to YES then the given
// enrollment token is valid.
- (void)configureAppWithEnrollmentTokenValid:(BOOL)enrollmentTokenValid {
  AppLaunchConfiguration config;

  // Show the First Run UI at startup.
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");

  // Pass the enrollment token to the app.
  config.additional_args.push_back("-CloudManagementEnrollmentToken");
  if (enrollmentTokenValid) {
    config.additional_args.push_back(kEnrollmentToken);
  } else {
    config.additional_args.push_back(policy::kInvalidEnrollmentToken);
  }

  // Pass the URL of the embedded test server to the app.
  config.additional_args.push_back(
      base::StrCat({"--", policy::switches::kDeviceManagementUrl, "=",
                    _policyTestServer->GetServiceURL().spec()}));

  config.relaunch_policy = ForceRelaunchByKilling;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

#pragma mark - Tests

// Ensures that the loading screen is dismissed when registration and policy
// fetch succeed.
// TODO(crbug.com/40255277): Flaky.
- (void)FLAKY_testLoadingScreenDismissedByPolicyFetchSuccess {
  [self configureAppWithEnrollmentTokenValid:YES];
  [self verifyLoadingScreenIsDismissed];
  GREYAssertTrue([PolicyAppInterface isCloudPolicyClientRegistered],
                 @"Cloud policy client is not registered.");
}

// Ensures that the loading screen is dismissed when registration fails.
// TODO(crbug.com/40255277): Flaky.
- (void)FLAKY_testLoadingScreenDismissedByRegisterFail {
  [self configureAppWithEnrollmentTokenValid:NO];
  [self verifyLoadingScreenIsDismissed];
  GREYAssertFalse(
      [PolicyAppInterface isCloudPolicyClientRegistered],
      @"Cloud policy client is registered with an invalid enrollment token.");
}

// Ensures that the loading screen is dismissed when registration succeeds and
// policy fetch fails.
// TODO(crbug.com/40254900): Flaky.
- (void)DISABLED_testLoadingScreenDismissedByPolicyFetchError {
  _policyTestServer->ConfigureRequestError(
      policy::dm_protocol::kValueRequestPolicy, net::HTTP_METHOD_NOT_ALLOWED);

  [self configureAppWithEnrollmentTokenValid:YES];

  [self verifyLoadingScreenIsDismissed];
  GREYAssertTrue([PolicyAppInterface isCloudPolicyClientRegistered],
                 @"Cloud policy client is not registered.");
}
@end

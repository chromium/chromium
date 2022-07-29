// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/earl_grey_test.h"

#import <memory>

#import "base/bind.h"
#import "base/strings/strcat.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/policy_switches.h"
#import "components/policy/proto/cloud_policy.pb.h"
#import "components/policy/test_support/embedded_policy_test_server.h"
#import "components/policy/test_support/policy_storage.h"
#import "components/policy/test_support/signature_provider.h"
#import "components/strings/grit/components_strings.h"
#import "google_apis/gaia/gaia_switches.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service_constants.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

std::string GetTestEmail() {
  return base::StrCat({"enterprise@", policy::SignatureProvider::kTestDomain1});
}

std::string GetTestDomain() {
  return std::string(policy::SignatureProvider::kTestDomain1);
}

// Handles the UserInfo fetch requests done during user policy fetch.
std::unique_ptr<net::test_server::HttpResponse> HandleUserInfoRequest(
    const net::test_server::HttpRequest& r) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content(base::StringPrintf(R"(
        {
          "email": "%s",
          "verified_email": true,
          "hd": "%s"
        })",
                                                GetTestEmail().c_str(),
                                                GetTestDomain().c_str()));
  return http_response;
}

// Sets up and starts the test policy server that handles the user policy
// requests.
void SetUpPolicyServer(policy::EmbeddedPolicyTestServer* policy_server) {
  policy::PolicyStorage* policy_storage = policy_server->policy_storage();

  // Set a policy value for testing.
  enterprise_management::CloudPolicySettings settings;
  settings.mutable_incognitomodeavailability()
      ->mutable_policy_options()
      ->set_mode(enterprise_management::PolicyOptions::MANDATORY);
  settings.mutable_incognitomodeavailability()->set_value(1);
  policy_storage->SetPolicyPayload(policy::dm_protocol::kChromeUserPolicyType,
                                   settings.SerializeAsString());

  policy_storage->add_managed_user("*");
  policy_storage->set_policy_user(GetTestEmail().c_str());
  policy_storage->signature_provider()->set_current_key_version(1);
  policy_storage->set_policy_invalidation_topic("test_policy_topic");
}

// Verifies from the UI and the policy store that the user policies are set.
void VerifyThatPoliciesAreSet() {
  // Verify that there is policy data in the store.
  GREYAssertTrue([PolicyAppInterface hasUserPolicyDataInCurrentBrowserState],
                 @"No user policy data found");
  // Verify that the policy is set.
  GREYAssertTrue(
      [PolicyAppInterface
          hasUserPolicyInCurrentBrowserState:@"IncognitoModeAvailability"
                            withIntegerValue:1],
      @"No policy data for IncognitoModeAvailability");
}

// Verifies from the UI and the policy store that the user policies are not set.
void VerifyThatPoliciesAreNotSet() {
  // Verify that there is no policy data in the store.
  GREYAssertFalse([PolicyAppInterface hasUserPolicyDataInCurrentBrowserState],
                  @"There should not be user policy data in the store");
}

}  // namespace

// Test suite for User Policy.
@interface UserPolicyTestCase : ChromeTestCase
@end

@implementation UserPolicyTestCase {
  std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_test_server_;
  std::unique_ptr<net::EmbeddedTestServer> embedded_test_server_;
}

- (void)setUp {
  // Set up and start the local policy test server.
  policy_test_server_ = std::make_unique<policy::EmbeddedPolicyTestServer>();
  SetUpPolicyServer(policy_test_server_.get());
  if (!policy_test_server_->Start()) {
    // Use NOTREACHED() instead of GREYAssertTrue because GREYAssertTrue can
    // only be used after calling the -setUp method of the super class.
    NOTREACHED();
  }

  // Set up and start the local test server for other services.
  embedded_test_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTP);
  embedded_test_server_->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/oauth2/v1/userinfo",
      base::BindRepeating(&HandleUserInfoRequest)));
  if (!embedded_test_server_->Start()) {
    // Use NOTREACHED() instead of GREYAssertTrue because GREYAssertTrue can
    // only be used after calling the -setUp method of the super class.
    NOTREACHED();
  }

  [super setUp];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.additional_args.push_back(
      base::StrCat({"--enable-user-policy-for-ios"}));
  // Set the url of the DMServer to reach the local test server.
  config.additional_args.push_back(
      base::StrCat({"--", policy::switches::kDeviceManagementUrl, "=",
                    policy_test_server_->GetServiceURL().spec()}));
  // Set the url of googleapis to reach the local test server.
  config.additional_args.push_back(
      base::StrCat({"--", switches::kGoogleApisUrl, "=",
                    embedded_test_server_->base_url().spec()}));
  return config;
}

// Tests that the user policies are fetched and activated when turning on Sync
// for a managed account.
- (void)testThatPoliciesAreFetchedWhenTurnOnSync {
  // Turn on Sync for managed account to fetch user policies.
  FakeChromeIdentity* fakeManagedIdentity = [FakeChromeIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail().c_str())
                 gaiaID:@"exampleManagedID"
                   name:@"Fake Managed"];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeManagedIdentity];

  VerifyThatPoliciesAreSet();
}

// Tests that the user policies are cleared after sign out.
- (void)testThatPoliciesAreClearedOnSignOut {
  // Turn on Sync for managed account to fetch user policies.
  FakeChromeIdentity* fakeManagedIdentity = [FakeChromeIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail().c_str())
                 gaiaID:@"exampleManagedID"
                   name:@"Fake Managed"];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeManagedIdentity];
  VerifyThatPoliciesAreSet();

  // Verify that the policies are cleared on sign out.
  [ChromeEarlGreyAppInterface signOutAndClearIdentities];
  VerifyThatPoliciesAreNotSet();
}

@end

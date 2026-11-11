// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <atomic>
#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/strcat.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/synchronization/lock.h"
#import "base/test/ios/wait_util.h"
#import "base/thread_annotations.h"
#import "base/time/time.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/core/common/cloud/cloud_policy_validator.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/policy/core/common/policy_switches.h"
#import "components/policy/policy_constants.h"
#import "components/policy/proto/cloud_policy.pb.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "components/policy/test_support/embedded_policy_test_server.h"
#import "components/policy/test_support/policy_storage.h"
#import "components/policy/test_support/request_handler_for_policy.h"
#import "components/policy/test_support/signature_provider.h"
#import "components/policy/test_support/test_server_helpers.h"
#import "components/strings/grit/components_strings.h"
#import "google_apis/gaia/gaia_switches.h"
#import "ios/chrome/browser/authentication/test/separate_profiles_util.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/test/signin_matchers.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/views/views_constants.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_constants.h"
#import "ios/chrome/browser/policy/model/policy_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/http/http_status_code.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using policy_test_utils::SetPolicy;

namespace {

namespace em = enterprise_management;

constexpr base::TimeDelta kWaitOnScheduledUserPolicyFetchInterval =
    base::Seconds(20);

// Timeout for a user policy fetch to be processed.
constexpr base::TimeDelta kUserPolicyFetchTimeout = base::Seconds(20);

// Values of the IncognitoModeAvailability policy. Both differ from the default
// value of the pref the policy is applied to, so that the pref shows whether
// the policy got applied.
constexpr int kIncognitoModeDisabled =
    static_cast<int>(policy::IncognitoModeAvailability::kDisabled);
constexpr int kIncognitoModeForced =
    static_cast<int>(policy::IncognitoModeAvailability::kForced);

// Token added to the user policy served with a bad signature. The client must
// send it back in its policy validation report.
constexpr char kBadSignaturePolicyToken[] = "bad_signature_policy_token";

// Error returned for malformed policy validation reports.
constexpr char kInvalidPolicyValidationReportError[] =
    "Invalid policy validation report.";

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

// Handles policy fetches like the default fake DMServer handler, but can serve
// the user policy with a bad signature: its `policy_data` is altered after
// being signed, as if it was tampered with in transit.
class BadSignaturePolicyRequestHandler
    : public policy::RequestHandlerForPolicy {
 public:
  explicit BadSignaturePolicyRequestHandler(
      policy::EmbeddedPolicyTestServer* parent)
      : policy::RequestHandlerForPolicy(parent) {}

  // Sets whether the user policy is served with a bad signature. Thread-safe,
  // as requests are handled on the server's IO thread.
  void set_serve_bad_signature(bool serve_bad_signature) {
    serve_bad_signature_ = serve_bad_signature;
  }

  // policy::RequestHandlerForPolicy:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override {
    std::unique_ptr<net::test_server::HttpResponse> response =
        policy::RequestHandlerForPolicy::HandleRequest(request);
    if (!serve_bad_signature_ || !response) {
      return response;
    }

    // The default handler builds all its responses with
    // `policy::CreateHttpResponse()`.
    auto* http_response =
        static_cast<net::test_server::BasicHttpResponse*>(response.get());
    em::DeviceManagementResponse dm_response;
    if (http_response->code() != net::HTTP_OK ||
        !dm_response.ParseFromString(http_response->content()) ||
        !dm_response.has_policy_response()) {
      return response;
    }

    bool tampered = false;
    for (em::PolicyFetchResponse& fetch_response :
         *dm_response.mutable_policy_response()->mutable_responses()) {
      em::PolicyData policy_data;
      if (!policy_data.ParseFromString(fetch_response.policy_data()) ||
          policy_data.policy_type() !=
              policy::dm_protocol::GetChromeUserPolicyType()) {
        continue;
      }
      // The client only reports validation failures of policies that have a
      // `policy_token`, which the fake DMServer doesn't set. Adding it after
      // signing is also what invalidates `policy_data_signature`.
      policy_data.set_policy_token(kBadSignaturePolicyToken);
      fetch_response.set_policy_data(policy_data.SerializeAsString());
      tampered = true;
    }
    if (!tampered) {
      return response;
    }
    return policy::CreateHttpResponse(net::HTTP_OK, dm_response);
  }

 private:
  std::atomic<bool> serve_bad_signature_{false};
};

// Records the policy validation reports uploaded to the fake DMServer, which
// doesn't handle them by default.
class PolicyValidationReportRecorder
    : public policy::EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit PolicyValidationReportRecorder(
      policy::EmbeddedPolicyTestServer* parent)
      : policy::EmbeddedPolicyTestServer::RequestHandler(parent) {}

  // Returns the number of reports received so far. Thread-safe, as requests
  // are handled on the server's IO thread.
  size_t report_count() const {
    base::AutoLock lock(lock_);
    return reports_.size();
  }

  // Returns the reports received so far. Thread-safe, as requests are handled
  // on the server's IO thread.
  std::vector<em::PolicyValidationReportRequest> reports() const {
    base::AutoLock lock(lock_);
    return reports_;
  }

  // policy::EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override {
    return policy::dm_protocol::kValueRequestUploadPolicyValidationReport;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override {
    em::DeviceManagementRequest dm_request;
    if (!dm_request.ParseFromString(request.content) ||
        !dm_request.has_policy_validation_report_request()) {
      return policy::CreateHttpResponse(net::HTTP_BAD_REQUEST,
                                        kInvalidPolicyValidationReportError);
    }
    {
      base::AutoLock lock(lock_);
      reports_.push_back(
          std::move(*dm_request.mutable_policy_validation_report_request()));
    }
    em::DeviceManagementResponse dm_response;
    dm_response.mutable_policy_validation_report_response();
    return policy::CreateHttpResponse(net::HTTP_OK, dm_response);
  }

 private:
  // Guards `reports_`, which is written on the server's IO thread and read on
  // the test's main thread. A lock is needed because the EG test process has
  // no task environment that the IO thread could post the reports to.
  mutable base::Lock lock_;
  std::vector<em::PolicyValidationReportRequest> reports_ GUARDED_BY(lock_);
};

// Sets the IncognitoModeAvailability user policy served by the fake DMServer.
void SetIncognitoModeAvailabilityPolicy(policy::PolicyStorage& policy_storage,
                                        int value) {
  em::CloudPolicySettings settings;
  settings.mutable_incognitomodeavailability()
      ->mutable_policy_options()
      ->set_mode(em::PolicyOptions::MANDATORY);
  settings.mutable_incognitomodeavailability()->set_value(value);
  policy_storage.SetPolicyPayload(
      policy::dm_protocol::GetChromeUserPolicyType(),
      settings.SerializeAsString());
}

// Sets up and starts the test policy server that handles the user policy
// requests.
void SetUpPolicyServer(policy::EmbeddedPolicyTestServer* policy_server) {
  policy::PolicyStorage* policy_storage = policy_server->policy_storage();

  // Set a policy value for testing.
  SetIncognitoModeAvailabilityPolicy(*policy_storage, kIncognitoModeDisabled);

  policy_storage->add_managed_user("*");
  policy_storage->set_policy_user(GetTestEmail());
  policy_storage->signature_provider()->set_current_key_version(1);
  policy_storage->set_policy_invalidation_topic("test_policy_topic");
}

// Waits on user policy data to be accessible from the Profile.
void WaitOnUserPolicy(base::TimeDelta timeout) {
  // Wait for user policy fetch.
  ConditionBlock condition = ^{
    return [PolicyAppInterface hasUserPolicyDataInCurrentProfile];
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(timeout, condition),
             @"No user policy data found");
}

// Verifies from the UI and the policy store that the user policies are set.
void VerifyThatPoliciesAreSet() {
  WaitOnUserPolicy(kWaitOnScheduledUserPolicyFetchInterval);

  // Verify that the policy is set.
  GREYAssertTrue([PolicyAppInterface
                     hasUserPolicyInCurrentProfile:@"IncognitoModeAvailability"
                                  withIntegerValue:kIncognitoModeDisabled],
                 @"No policy data for IncognitoModeAvailability");
}

// Verifies from the UI and the policy store that the user policies are not set.
void VerifyThatPoliciesAreNotSet() {
  // Verify that there is no policy data in the store.
  GREYAssertFalse([PolicyAppInterface hasUserPolicyDataInCurrentProfile],
                  @"There should not be user policy data in the store");
}

// Waits until the user policy store has status `STATUS_OK` and the
// IncognitoModeAvailability policy is `value`, both in the store and in the
// pref it's applied to.
void WaitForIncognitoModeAvailabilityPolicy(int value) {
  NSString* policy_name =
      base::SysUTF8ToNSString(policy::key::kIncognitoModeAvailability);
  ConditionBlock condition = ^{
    return
        [PolicyAppInterface userPolicyStoreStatusInCurrentProfile] ==
            policy::CloudPolicyStore::STATUS_OK &&
        [PolicyAppInterface hasUserPolicyInCurrentProfile:policy_name
                                         withIntegerValue:value] &&
        [ChromeEarlGrey
            userIntegerPref:policy::policy_prefs::kIncognitoModeAvailability] ==
            value;
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(kUserPolicyFetchTimeout,
                                                   condition),
      @"IncognitoModeAvailability user policy wasn't applied with value %d "
      @"(store status: %ld, store has value: %@, pref value: %d)",
      value,
      static_cast<long>(
          [PolicyAppInterface userPolicyStoreStatusInCurrentProfile]),
      [PolicyAppInterface hasUserPolicyInCurrentProfile:policy_name
                                       withIntegerValue:value]
          ? @"YES"
          : @"NO",
      [ChromeEarlGrey
          userIntegerPref:policy::policy_prefs::kIncognitoModeAvailability]);
}

// Waits until the user policy store has rejected a fetched policy because of
// its signature.
void WaitForUserPolicyRejectedWithBadSignature() {
  ConditionBlock condition = ^{
    return [PolicyAppInterface userPolicyStoreStatusInCurrentProfile] ==
               policy::CloudPolicyStore::STATUS_VALIDATION_ERROR &&
           [PolicyAppInterface userPolicyValidationStatusInCurrentProfile] ==
               policy::CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE;
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(kUserPolicyFetchTimeout,
                                                   condition),
      @"User policy wasn't rejected because of its signature (store status: "
      @"%ld, validation status: %ld)",
      static_cast<long>(
          [PolicyAppInterface userPolicyStoreStatusInCurrentProfile]),
      static_cast<long>(
          [PolicyAppInterface userPolicyValidationStatusInCurrentProfile]));
}

// Waits until `recorder` has received at least one policy validation report.
void WaitForPolicyValidationReport(
    const PolicyValidationReportRecorder& recorder) {
  ConditionBlock condition = ^{
    return recorder.report_count() > 0;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kUserPolicyFetchTimeout, condition),
             @"No policy validation report was received");
}

void ClearUserPolicyPrefs() {
  // Clears the user policy notification pref.
  [ChromeEarlGrey clearUserPrefWithName:policy::policy_prefs::
                                            kUserPolicyNotificationWasShown];
  // Clears the pref used used to determine the fetch interval.
  [ChromeEarlGrey
      clearUserPrefWithName:policy::policy_prefs::kLastPolicyCheckTime];
  [ChromeEarlGrey commitPendingUserPrefsWrite];
}

id<GREYMatcher> ManagedProfileCreationTitleMatcher() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_TITLE));
}

id<GREYMatcher> ManagedProfileCreationSubtitleMatcher() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_SUBTITLE));
}

// Returns a matcher for the sign-in screen "Cancel" button.
id<GREYMatcher> DeclineManagementButtonMatcher() {
  return grey_allOf(chrome_test_util::ButtonStackSecondaryButton(),
                    grey_sufficientlyVisible(), nil);
}

}  // namespace

// Test suite for User Policy.
@interface UserPolicyTestCase : ChromeTestCase
@end

@implementation UserPolicyTestCase {
  std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_test_server_;
  std::unique_ptr<net::EmbeddedTestServer> embedded_test_server_;
  // Owned by `policy_test_server_`.
  raw_ptr<BadSignaturePolicyRequestHandler> policy_request_handler_;
  raw_ptr<PolicyValidationReportRecorder> policy_validation_report_recorder_;
}

- (void)setUp {
  // Set up and start the local policy test server.
  policy_test_server_ = std::make_unique<policy::EmbeddedPolicyTestServer>();
  SetUpPolicyServer(policy_test_server_.get());
  // Register the handlers before the server starts, as they are read on its IO
  // thread.
  auto policy_request_handler =
      std::make_unique<BadSignaturePolicyRequestHandler>(
          policy_test_server_.get());
  policy_request_handler_ = policy_request_handler.get();
  policy_test_server_->RegisterHandler(std::move(policy_request_handler));
  auto policy_validation_report_recorder =
      std::make_unique<PolicyValidationReportRecorder>(
          policy_test_server_.get());
  policy_validation_report_recorder_ = policy_validation_report_recorder.get();
  policy_test_server_->RegisterHandler(
      std::move(policy_validation_report_recorder));
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

- (void)tearDownHelper {
  ClearUserPolicyPrefs();
  [PolicyAppInterface clearPolicies];
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  return [self minimalAppConfigurationForTestCase];
}

- (AppLaunchConfiguration)minimalAppConfigurationForTestCase {
  AppLaunchConfiguration config;
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

#pragma mark - Tests

// Tests that the user policies are fetched and activated when signing in with
// a managed account.
- (void)testThatPoliciesAreFetchedOnSignIn {
  // Sign in with managed account to fetch user policies.
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail())];
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:fakeManagedIdentity];

  VerifyThatPoliciesAreSet();
}

// Tests that the user policies are cleared after sign out.
- (void)testThatPoliciesAreClearedOnSignOut {
  // Sign in with managed account to fetch user policies.
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail())];
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:fakeManagedIdentity];
  VerifyThatPoliciesAreSet();

  // Verify that the policies are cleared on sign out.
  [ChromeEarlGrey signOutAndClearIdentities];
  VerifyThatPoliciesAreNotSet();
}

// Tests that the user policies previously fetched are loaded from the store
// when signed in at startup.
- (void)testThatPoliciesAreLoadedFromStoreWhenSignedInAtStartup {
  // Sign in with managed account to fetch user policies.
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail())];
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:fakeManagedIdentity];

  VerifyThatPoliciesAreSet();

  // Restart the browser while keeping sign-in by preserving the identity of the
  // managed account.
  AppLaunchConfiguration config = [self minimalAppConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      std::string("-") + test_switches::kAddFakeIdentitiesAtStartup + "=" +
      [FakeSystemIdentity encodeIdentitiesToBase64:@[ fakeManagedIdentity ]]);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Wait until the user policies are loaded from disk.
  WaitOnUserPolicy(kWaitOnScheduledUserPolicyFetchInterval);

  // Verifiy that the policies that were fetched in the previous session are
  // loaded from the cache at startup.
  VerifyThatPoliciesAreSet();
}

// Tests that a fetched user policy with a bad signature is rejected without
// clearing the policies in place, that the rejection is reported to DMServer,
// and that the next valid fetch is applied without restarting the app.
- (void)testThatBadSignatureIsReportedAndRecoveredWithoutRestart {
  // Sign in with managed account to fetch user policies.
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail())];
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:fakeManagedIdentity];
  WaitForIncognitoModeAvailabilityPolicy(kIncognitoModeDisabled);

  // Serve a different value with a bad signature, so that applying the
  // rejected policy would be visible.
  policy_request_handler_->set_serve_bad_signature(true);
  SetIncognitoModeAvailabilityPolicy(*policy_test_server_->policy_storage(),
                                     kIncognitoModeForced);
  GREYAssertTrue([PolicyAppInterface refreshUserPolicyInCurrentProfile],
                 @"User policy client isn't registered");
  WaitForUserPolicyRejectedWithBadSignature();
  WaitForPolicyValidationReport(*policy_validation_report_recorder_);

  // The policy applied before the rejection is kept.
  GREYAssertTrue([PolicyAppInterface hasUserPolicyDataInCurrentProfile],
                 @"User policy was cleared");
  GREYAssertTrue(
      [PolicyAppInterface
          hasUserPolicyInCurrentProfile:
              base::SysUTF8ToNSString(policy::key::kIncognitoModeAvailability)
                       withIntegerValue:kIncognitoModeDisabled],
      @"User policy from the rejected fetch was stored");
  GREYAssertEqual(
      [ChromeEarlGrey
          userIntegerPref:policy::policy_prefs::kIncognitoModeAvailability],
      kIncognitoModeDisabled, @"Applied policy changed after the rejection");

  // The report identifies the rejected policy and the failure reason.
  const std::vector<em::PolicyValidationReportRequest> reports =
      policy_validation_report_recorder_->reports();
  GREYAssertEqual(reports.size(), 1u, @"Unexpected number of reports");
  const em::PolicyValidationReportRequest& report = reports[0];
  GREYAssertEqual(
      report.validation_result_type(),
      em::PolicyValidationReportRequest::VALIDATION_RESULT_TYPE_BAD_SIGNATURE,
      @"Unexpected validation result type");
  GREYAssertEqual(report.action(), em::PolicyValidationReportRequest::STORE,
                  @"Unexpected validation action");
  GREYAssertEqualObjects(
      base::SysUTF8ToNSString(report.policy_type()),
      base::SysUTF8ToNSString(policy::dm_protocol::GetChromeUserPolicyType()),
      @"Unexpected policy type");
  GREYAssertEqualObjects(base::SysUTF8ToNSString(report.policy_token()),
                         base::SysUTF8ToNSString(kBadSignaturePolicyToken),
                         @"Report doesn't identify the rejected policy");

  // The same app instance applies the next valid policy.
  policy_request_handler_->set_serve_bad_signature(false);
  GREYAssertTrue([PolicyAppInterface refreshUserPolicyInCurrentProfile],
                 @"User policy client isn't registered");
  WaitForIncognitoModeAvailabilityPolicy(kIncognitoModeForced);
  GREYAssertEqual(
      [PolicyAppInterface userPolicyValidationStatusInCurrentProfile],
      static_cast<NSInteger>(policy::CloudPolicyValidatorBase::VALIDATION_OK),
      @"User policy validation didn't recover");
}

// Tests that the managed accout confirmation dialog is shown in the sign-in
// flow with its contextual and specific content.
- (void)testSigninFlowConfirmationDialogWhenUserPolicyAndSignin {
  AppLaunchConfiguration config = [self minimalAppConfigurationForTestCase];
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail())];

  [SigninEarlGrey addFakeIdentity:fakeManagedIdentity];

  [self startSigninFlowUpToConfirmationDialogWithIdentity:fakeManagedIdentity];

  // Disable egtest synchronization to avoid infinite spinner loop.
  ScopedSynchronizationDisabler disabler;

  WaitForEnterpriseOnboardingScreen();

  // Verify the management disclosure UI.
  [ChromeEarlGrey waitForMatcher:ManagedProfileCreationTitleMatcher()];
  [[EarlGrey selectElementWithMatcher:ManagedProfileCreationTitleMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ManagedProfileCreationSubtitleMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:DeclineManagementButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Complete the sign-in flow by completing the confirmation dialog.
  ConfirmEnterpriseOnboardingScreen();

  // Verify that the confirmation dialog was dismissed.
  WaitForEnterpriseOnboardingScreenToDisappear();

  // Verify that the flow was successful by validating that the account is
  // signed in after accepting from the confirmation dialog.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeManagedIdentity];
}

// Tests that when user policies are enabled, in the sign-in flow, sign-in error
// popup isn't shown after cancelling the managed accout confirmation dialog.
- (void)testCancelSigninFlowConfirmationDialogWhenUserPolicyAndSignin {
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail())];

  [SigninEarlGrey addFakeIdentity:fakeManagedIdentity];

  [self startSigninFlowUpToConfirmationDialogWithIdentity:fakeManagedIdentity];

  // Disable egtest synchronization to avoid infinite spinner loop.
  ScopedSynchronizationDisabler disabler;

  WaitForEnterpriseOnboardingScreen();

  // Cancel the sign-in flow by tapping on the cancel button.
  [ChromeEarlGrey waitForMatcher:DeclineManagementButtonMatcher()];
  [[EarlGrey selectElementWithMatcher:DeclineManagementButtonMatcher()]
      performAction:grey_tap()];

  // Verify that the confirmation dialog was dismissed.
  WaitForEnterpriseOnboardingScreenToDisappear();

  // Verify that no sign-in error alert action is shown.
  [ChromeEarlGrey
      waitForMatcher:chrome_test_util::ConsistencySigninPrimaryButtonMatcher()];
  NSString* errorTitle = l10n_util::GetNSString(IDS_IOS_WEBSIGN_ERROR_TITLE);
  [[EarlGrey selectElementWithMatcher:grey_text(errorTitle)]
      assertWithMatcher:grey_notVisible()];

  // Verify that the flow was cancelled by validating that the account is
  // not signed in after accepting from the confirmation dialog.
  [SigninEarlGrey verifySignedOut];
}

// Tests the chrome://policy page when there are user level policies.
- (void)testPolicyPageManagedWithUserPolicy {
  // Sign in with a managed account.
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail())];
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:fakeManagedIdentity];
  VerifyThatPoliciesAreSet();

  // Open the policy page and check if there is a user management status box.
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_STATUS_USER)];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_LABEL_USERNAME)];
  [ChromeEarlGrey waitForWebStateContainingText:GetTestEmail()];
}

#pragma mark - Private

// Starts the sign-in flow up to the point where it may ask for the confirmation
// dialog.
- (void)startSigninFlowUpToConfirmationDialogWithIdentity:
    (FakeSystemIdentity*)identity {
  // Open sign-in flow.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsSignInRowMatcher()];

  // Proceed with sign-in.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          chrome_test_util::ConsistencySigninPrimaryButtonMatcher()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ConsistencySigninPrimaryButtonMatcher()]
      performAction:grey_tap()];
}

@end

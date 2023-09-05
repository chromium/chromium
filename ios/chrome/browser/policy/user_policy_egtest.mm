// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/earl_grey_test.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/strcat.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/policy/core/common/policy_switches.h"
#import "components/policy/proto/cloud_policy.pb.h"
#import "components/policy/test_support/embedded_policy_test_server.h"
#import "components/policy/test_support/policy_storage.h"
#import "components/policy/test_support/signature_provider.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "google_apis/gaia/gaia_switches.h"
#import "ios/chrome/browser/policy/cloud/user_policy_constants.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

constexpr base::TimeDelta kWaitOnScheduledUserPolicyFetchInterval =
    base::Seconds(20);

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

void ClearUserPolicyPrefs() {
  // Clears the user policy notification pref.
  [ChromeEarlGreyAppInterface
      clearUserPrefWithName:
          base::SysUTF8ToNSString(
              policy::policy_prefs::kUserPolicyNotificationWasShown)];
  // Clears the pref used used to determine the fetch interval.
  [ChromeEarlGreyAppInterface
      clearUserPrefWithName:base::SysUTF8ToNSString(
                                policy::policy_prefs::kLastPolicyCheckTime)];
  [ChromeEarlGreyAppInterface commitPendingUserPrefsWrite];
}

void WaitOnUserPolicy(base::TimeDelta timeout) {
  // Wait for user policy fetch.
  ConditionBlock condition = ^{
    return [PolicyAppInterface hasUserPolicyDataInCurrentBrowserState];
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(timeout, condition),
             @"Didn't fetch user policies");
}

void VerifyTheNotificationUI() {
  // Swipe up to make sure that all the text content in the prompt is visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kConfirmationAlertTitleAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_USER_POLICY_NOTIFICATION_NO_SIGNOUT_TITLE);
  NSString* subtitle = l10n_util::GetNSStringF(
      IDS_IOS_USER_POLICY_NOTIFICATION_NO_SIGNOUT_SUBTITLE,
      base::UTF8ToUTF16(std::string(policy::SignatureProvider::kTestDomain1)));

  // Verify the notification UI.
  [[EarlGrey selectElementWithMatcher:grey_text(title)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_text(subtitle)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Wait for the chrome management url to become visible in the web state
// without validating the content. The goal being to verify that the page was
// opened.
void WaitForVisibleChromeManagementURL() {
  // const GURL expectedURL(base::StrCat({kChromeUIManagementURL, "/"}));

  NSString* errorString = [NSString
      stringWithFormat:@"Failed waiting for web state"
                       @" with visible url %@ ",
                       base::SysUTF8ToNSString(kChromeUIManagementURL)];

  GREYCondition* waitForUrl = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    return base::StartsWith(
                        [ChromeEarlGrey webStateVisibleURL].spec(),
                        kChromeUIManagementURL);
                  }];
  base::TimeDelta timeout = base::Seconds(5);
  bool visibleUrl = [waitForUrl waitWithTimeout:timeout.InSecondsF()];
  GREYAssert(visibleUrl, errorString);
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

- (void)tearDown {
  ClearUserPolicyPrefs();
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [self minimalAppConfigurationForTestCase];
  // Enable User Policy for both consent levels.
  config.features_enabled.push_back(
      policy::kUserPolicyForSigninAndNoSyncConsentLevel);
  config.features_enabled.push_back(
      policy::kUserPolicyForSigninOrSyncConsentLevel);
  // Disable UNO to not interfere with the tests that need Sync.
  config.features_disabled.push_back(
      syncer::kReplaceSyncPromosWithSignInPromos);
  return config;
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

// Tests that the user policies are fetched and activated when turning on Sync
// for a managed account.
- (void)testThatPoliciesAreFetchedOnSigninAndSync {
  // Turn on Sync for managed account to fetch user policies.
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail().c_str())
                 gaiaID:@"exampleManagedID"
                   name:@"Fake Managed"];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeManagedIdentity];

  VerifyThatPoliciesAreSet();
}

// Tests that the user policies are fetched and activated when signed in without
// sync with a managed account.
- (void)testThatPoliciesAreFetchedOnSignInWithoutSync {
  // Turn on Sync for managed account to fetch user policies.
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail().c_str())
                 gaiaID:@"exampleManagedID"
                   name:@"Fake Managed"];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeManagedIdentity enableSync:NO];

  VerifyThatPoliciesAreSet();
}

// Tests that the user policies are fetched and activated when signed in without
// sync with a managed account and feature enabled for signed-in without sync.
- (void)testThatPoliciesAreFetchedOnSignInAndSigninNoSyncFeature {
  AppLaunchConfiguration config = [self minimalAppConfigurationForTestCase];
  // Enable User Policy for sign-in consent level exclusively.
  config.features_enabled.push_back(
      policy::kUserPolicyForSigninAndNoSyncConsentLevel);
  // Disable UNO as fetching user policies don't depend on UNO.
  config.features_disabled.push_back(
      syncer::kReplaceSyncPromosWithSignInPromos);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Turn on Sync for managed account to fetch user policies.
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail().c_str())
                 gaiaID:@"exampleManagedID"
                   name:@"Fake Managed"];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeManagedIdentity enableSync:NO];

  VerifyThatPoliciesAreSet();
}

// Tests that the user policies are not fetched when signed-in+sync and
// kReplaceSyncPromosWithSignInPromos is disabled. The feature is only
// enabled for sign-in without sync.
- (void)testThatPoliciesAreNotFetchedOnSigninWithSyncButSigninNoSyncFeature {
  AppLaunchConfiguration config = [self minimalAppConfigurationForTestCase];
  // Enable User Policy for sign-in consent level exclusively.
  config.features_enabled.push_back(
      policy::kUserPolicyForSigninAndNoSyncConsentLevel);
  // Disable UNO as fetching user policies don't depend on UNO.
  config.features_disabled.push_back(
      syncer::kReplaceSyncPromosWithSignInPromos);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Turn on Sync for managed account to attempt to fetch user policies.
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail().c_str())
                 gaiaID:@"exampleManagedID"
                   name:@"Fake Managed"];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeManagedIdentity enableSync:YES];

  // Wait enough time to verifiy that the fetch wasn't unexpectedly triggered.
  base::test::ios::SpinRunLoopWithMinDelay(
      kWaitOnScheduledUserPolicyFetchInterval);

  VerifyThatPoliciesAreNotSet();
}

// Tests that the user policies are cleared after sign out.
- (void)testThatPoliciesAreClearedOnSignOut {
  // Turn on Sync for managed account to fetch user policies.
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail().c_str())
                 gaiaID:@"exampleManagedID"
                   name:@"Fake Managed"];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeManagedIdentity];
  VerifyThatPoliciesAreSet();

  // Verify that the policies are cleared on sign out.
  [ChromeEarlGreyAppInterface signOutAndClearIdentitiesWithCompletion:nil];
  VerifyThatPoliciesAreNotSet();
}

// Tests that the user policies previously fetched are loaded from the store
// when signed in at startup.
- (void)testThatPoliciesAreLoadedFromStoreWhenSignedInAtStartup {
  // Turn on Sync for managed account to fetch user policies.
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail().c_str())
                 gaiaID:@"exampleManagedID"
                   name:@"Fake Managed"];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeManagedIdentity enableSync:NO];

  VerifyThatPoliciesAreSet();

  // Restart the browser while keeping sign-in by preserving the identity of the
  // managed account.
  AppLaunchConfiguration config = [self minimalAppConfigurationForTestCase];
  // Enable User Policy for sign-in consent level exclusively.
  config.features_enabled.push_back(
      policy::kUserPolicyForSigninAndNoSyncConsentLevel);
  // Disable UNO as fetching user policies don't depend on UNO.
  config.features_disabled.push_back(
      syncer::kReplaceSyncPromosWithSignInPromos);
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      base::StrCat({"--", test_switches::kSignInAtStartup}));
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

// TODO(crbug.com/1386163): Tests that the user policies are fetched when the
// user decides to "Continue" in the notification dialog.
- (void)DISABLED_testUserPolicyNotificationWithAcceptChoice {
  // Clear the prefs related to user policy to make sure that the notification
  // isn't skipped and that the fetch is started within the minimal schedule
  // interval.
  ClearUserPolicyPrefs();

  // Restart the app to disable user policy and allow turning on Sync for the
  // managed account.
  AppLaunchConfiguration config;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Turn on Sync for managed account. This won't trigger the user policy fetch.
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail().c_str())
                 gaiaID:@"exampleManagedID"
                   name:@"Fake Managed"];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeManagedIdentity];

  [ChromeEarlGreyAppInterface commitPendingUserPrefsWrite];

  // Restart the browser while keeping Sync ON by preserving the identity of the
  // managed account.
  config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      base::StrCat({"--", test_switches::kSignInAtStartup}));
  config.additional_args.push_back(
      std::string("-") + test_switches::kAddFakeIdentitiesAtStartup + "=" +
      [FakeSystemIdentity encodeIdentitiesToBase64:@[ fakeManagedIdentity ]]);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Verify that the notification dialog is there.
  VerifyTheNotificationUI();

  // Tap on the "Continue" button to dismiss the alert dialog and start the user
  // policy fetch.
  NSString* continueLabel =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT_CONTINUE);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(continueLabel),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];

  // Wait for user policy fetch. This will take at least 5 seconds which
  // corresponds to the minimal user policy fetch delay when triggering the
  // fetch at startup.
  WaitOnUserPolicy(kWaitOnScheduledUserPolicyFetchInterval);

  // Verifiy that the policies were fetched and loaded.
  VerifyThatPoliciesAreSet();
}

// Tests that the learn more page is displayed when choosing that option in the
// notice dialog.
// TODO(crbug.com/1478990): reenable this test.
- (void)DISABLED_testUserPolicyNotificationWithLearnMoreChoice {
  // Clear the prefs related to user policy to make sure that the notification
  // isn't skipped and that the fetch is started within the minimal schedule
  // interval.
  ClearUserPolicyPrefs();

  // Restart the app to disable user policy and allow turning on Sync for the
  // managed account.
  AppLaunchConfiguration config;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Turn on Sync for managed account. This won't trigger the user policy fetch.
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail().c_str())
                 gaiaID:@"exampleManagedID"
                   name:@"Fake Managed"];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeManagedIdentity enableSync:NO];

  // Restart the browser while keeping sign-in by preserving the identity of the
  // managed account.
  config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      base::StrCat({"--", test_switches::kSignInAtStartup}));
  config.additional_args.push_back(
      std::string("-") + test_switches::kAddFakeIdentitiesAtStartup + "=" +
      [FakeSystemIdentity encodeIdentitiesToBase64:@[ fakeManagedIdentity ]]);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Verify that the notification dialog is there.
  VerifyTheNotificationUI();

  // Tap on the "Sign Out and Clear Data" button to dismiss the alert dialog
  // without triggering the user policy fetch.
  NSString* label =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_SIGNED_OUT_LEARN_MORE);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(label),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitButton),
                                          nil)] performAction:grey_tap()];

  WaitForVisibleChromeManagementURL();
}

// Tests that the managed accout confirmation dialog is shown in the
// sign-in+sync flow with its contextual and specific content when user policies
// are enabled.
- (void)testSigninFlowConfirmationDialogWhenUserPolicyAndSync {
  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail().c_str())
                 gaiaID:@"exampleManagedID"
                   name:@"Fake Managed"];
  [SigninEarlGrey addFakeIdentity:fakeManagedIdentity];

  [self startSigninFlowUpToConfirmationDialogWithIdentity:fakeManagedIdentity
                                              withoutSync:NO];

  ScopedSynchronizationDisabler disabler;

  NSString* title = l10n_util::GetNSString(IDS_IOS_MANAGED_SYNC_TITLE);
  NSString* subtitle = l10n_util::GetNSStringF(
      IDS_IOS_MANAGED_SYNC_WITH_USER_POLICY_SUBTITLE,
      base::UTF8ToUTF16(std::string(policy::SignatureProvider::kTestDomain1)));

  // Verify the notification UI.
  [[EarlGrey selectElementWithMatcher:grey_text(title)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_text(subtitle)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(@"CancelAlertAction"),
                            [ChromeMatchersAppInterface
                                buttonWithAccessibilityLabelID:IDS_CANCEL],
                            nil)] assertWithMatcher:grey_sufficientlyVisible()];

  // Complete the sign-in+sync flow by completing the dialog.
  id<GREYMatcher> acceptButton = [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON];
  [ChromeEarlGrey waitForMatcher:acceptButton];
  [[EarlGrey selectElementWithMatcher:acceptButton] performAction:grey_tap()];

  // Verify that the confirmation dialog was dismissed.
  [[EarlGrey selectElementWithMatcher:grey_text(title)]
      assertWithMatcher:grey_notVisible()];

  // Verify that the flow worked and the browser is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeManagedIdentity];
}

// Tests that the managed accout confirmation dialog is shown in the
// sign-in without sync flow with its contextual and specific content when user
// policies are enabled.
- (void)testSigninFlowConfirmationDialogWhenUserPolicyAndSigninWithoutSync {
  AppLaunchConfiguration config = [self minimalAppConfigurationForTestCase];
  // Enable User Policy for sign-in consent level exclusively.
  config.features_enabled.push_back(
      policy::kUserPolicyForSigninAndNoSyncConsentLevel);
  // Enable UNO to enable confirmation dialog for sign-in consent level.
  config.features_enabled.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail().c_str())
                 gaiaID:@"exampleManagedID"
                   name:@"Fake Managed"];

  [SigninEarlGrey addFakeIdentity:fakeManagedIdentity];

  [self startSigninFlowUpToConfirmationDialogWithIdentity:fakeManagedIdentity
                                              withoutSync:YES];

  // Disable egtest synchronization to avoid infinite spinner loop.
  ScopedSynchronizationDisabler disabler;

  NSString* title = l10n_util::GetNSString(IDS_IOS_MANAGED_SIGNIN_TITLE);
  NSString* subtitle = l10n_util::GetNSStringF(
      IDS_IOS_MANAGED_SIGNIN_WITH_USER_POLICY_SUBTITLE,
      base::UTF8ToUTF16(std::string(policy::SignatureProvider::kTestDomain1)));

  // Verify the notification UI.
  [[EarlGrey selectElementWithMatcher:grey_text(title)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_text(subtitle)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(@"CancelAlertAction"),
                            [ChromeMatchersAppInterface
                                buttonWithAccessibilityLabelID:IDS_CANCEL],
                            nil)] assertWithMatcher:grey_sufficientlyVisible()];

  // Complete the sign-in flow by completing the confirmation dialog.
  id<GREYMatcher> acceptButton = [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:
          IDS_IOS_MANAGED_SIGNIN_WITH_USER_POLICY_CONTINUE_BUTTON_LABEL];
  [ChromeEarlGrey waitForMatcher:acceptButton];
  [[EarlGrey selectElementWithMatcher:acceptButton] performAction:grey_tap()];

  // Verify that the confirmation dialog was dismissed.
  [[EarlGrey selectElementWithMatcher:grey_text(title)]
      assertWithMatcher:grey_notVisible()];

  // Verify that the flow was successful by validating that the account is
  // signed in after accepting from the confirmation dialog.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeManagedIdentity];
}

// Tests that the managed accout confirmation dialog is shown in the
// legacy sign-in+sync flow with its contextual and specific content when user
// policies are disabled.
- (void)testSigninFlowConfirmationDialogWhenNoUserPolicyAndLegacySyncPromo {
  AppLaunchConfiguration config = [self minimalAppConfigurationForTestCase];
  // Disable User Policy and UNO to get the legacy confirmation dialog.
  config.features_disabled.push_back(
      policy::kUserPolicyForSigninAndNoSyncConsentLevel);
  config.features_disabled.push_back(
      policy::kUserPolicyForSigninOrSyncConsentLevel);
  config.features_disabled.push_back(
      syncer::kReplaceSyncPromosWithSignInPromos);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  FakeSystemIdentity* fakeManagedIdentity = [FakeSystemIdentity
      identityWithEmail:base::SysUTF8ToNSString(GetTestEmail().c_str())
                 gaiaID:@"exampleManagedID"
                   name:@"Fake Managed"];

  [SigninEarlGrey addFakeIdentity:fakeManagedIdentity];

  [self startSigninFlowUpToConfirmationDialogWithIdentity:fakeManagedIdentity
                                              withoutSync:NO];

  // Disable egtest synchronization to avoid infinite spinner loop.
  ScopedSynchronizationDisabler disabler;

  NSString* title = l10n_util::GetNSString(IDS_IOS_MANAGED_SIGNIN_TITLE);
  NSString* subtitle = l10n_util::GetNSStringF(
      IDS_IOS_MANAGED_SIGNIN_SUBTITLE,
      base::UTF8ToUTF16(std::string(policy::SignatureProvider::kTestDomain1)));

  // Verify the notification UI.
  [[EarlGrey selectElementWithMatcher:grey_text(title)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_text(subtitle)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(@"CancelAlertAction"),
                            [ChromeMatchersAppInterface
                                buttonWithAccessibilityLabelID:IDS_CANCEL],
                            nil)] assertWithMatcher:grey_sufficientlyVisible()];

  // Complete the sign-in+sync flow by completing the confirmation dialog.
  id<GREYMatcher> acceptButton = [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON];
  [ChromeEarlGrey waitForMatcher:acceptButton];
  [[EarlGrey selectElementWithMatcher:acceptButton] performAction:grey_tap()];

  // Verify that the confirmation dialog was dismissed.
  [[EarlGrey selectElementWithMatcher:grey_text(title)]
      assertWithMatcher:grey_notVisible()];

  // Verify that the account is signed in after accepting from the confirmation
  // dialog.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeManagedIdentity];
}

#pragma mark - Private

// Starts the sign-in flow up to the point where it may ask for the confirmation
// dialog.
- (void)startSigninFlowUpToConfirmationDialogWithIdentity:
            (FakeSystemIdentity*)identity
                                              withoutSync:(BOOL)withoutSync {
  // Open sign-in flow.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsSignInRowMatcher()];

  // Proceed with sign-in.
  if (withoutSync) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                            WebSigninPrimaryButtonMatcher()]
        performAction:grey_tap()];

  } else {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kIdentityButtonControlIdentifier)]
        performAction:grey_tap()];
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::IdentityCellMatcherForEmail(
                                     identity.userEmail)]
        performAction:grey_tap()];
    [SigninEarlGreyUI tapSigninConfirmationDialog];
  }
}

@end

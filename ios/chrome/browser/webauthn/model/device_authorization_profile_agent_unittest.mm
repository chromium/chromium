// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/device_authorization_profile_agent.h"

#import <memory>
#import <string>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_features.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_types.h"
#import "components/webauthn/core/browser/device_authorization/mock_device_authorization_service.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/webauthn/model/ios_device_authorization_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::base::test::RunOnceCallback;
using ::base::test::RunOnceCallbackRepeatedly;

constexpr char kEmail[] = "user@gmail.com";
constexpr char kOtherEmail[] = "other@gmail.com";

// Smallest time step used to check the boundaries of the backoff.
constexpr base::TimeDelta kEpsilon = base::Seconds(1);

webauthn::DeviceAuthFetchResult SuccessResult() {
  return {webauthn::DeviceAuthorizationKeys()};
}

webauthn::DeviceAuthFetchResult ReAuthRequiredResult() {
  return {webauthn::DeviceAuthorizationReAuthParams()};
}

webauthn::DeviceAuthFetchResult ErrorResult() {
  return {};
}

std::unique_ptr<KeyedService> BuildMockDeviceAuthorizationService(
    ProfileIOS* profile) {
  auto service = std::make_unique<webauthn::MockDeviceAuthorizationService>();
  ON_CALL(*service, GetOrFetchKeys)
      .WillByDefault(RunOnceCallbackRepeatedly<0>(ErrorResult()));
  return service;
}

}  // namespace

class DeviceAuthorizationProfileAgentTest : public PlatformTest {
 protected:
  DeviceAuthorizationProfileAgentTest() {
    feature_list_.InitWithFeatures(
        {webauthn::features::kFetchDeviceAuthorizationKeys,
         webauthn::features::kDeviceAuthorizationStartupSilentFetch},
        {});

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(
        IOSDeviceAuthorizationServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockDeviceAuthorizationService));
    profile_ = std::move(builder).Build();
  }

  ~DeviceAuthorizationProfileAgentTest() override { DetachAgent(); }

  // Attaches an agent to a new `ProfileState`, detaching the previous one.
  void AttachAgent() {
    DetachAgent();
    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    profile_state_.profile = profile_.get();
    agent_ = [[DeviceAuthorizationProfileAgent alloc] init];
    [profile_state_ addAgent:agent_];
  }

  // Simulates an app launch: attaches an agent, moves its `ProfileState` to
  // `ProfileInitStage::kFinal` and runs the deferred fetch.
  void LaunchApp() {
    AttachAgent();
    SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
    RunDeferredFetch();
  }

  void DetachAgent() {
    [profile_state_.deferredRunner cancelAllBlocks];
    [profile_state_ removeAgent:agent_];
    profile_state_ = nil;
    agent_ = nil;
  }

  // Runs the deferred fetch block synchronously, if it is pending.
  void RunDeferredFetch() {
    [profile_state_.deferredRunner
        runBlockNamed:kDeviceAuthorizationFetchBlockName];
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile_.get());
  }

  // Makes `email` the primary account, replacing the current one if any.
  void SignIn(const std::string& email) {
    if (identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      signin::ClearPrimaryAccount(identity_manager());
    }
    signin::MakePrimaryAccountAvailable(identity_manager(), email,
                                        signin::ConsentLevel::kSignin);
  }

  // Expects the next fetch to be due exactly `delay` from now, checking on app
  // launches.
  void ExpectNextFetchDueAfter(base::TimeDelta delay) {
    EXPECT_CALL(*service(), GetOrFetchKeys).Times(0);
    task_environment_.FastForwardBy(delay - kEpsilon);
    LaunchApp();
    testing::Mock::VerifyAndClearExpectations(service());

    EXPECT_CALL(*service(), GetOrFetchKeys);
    task_environment_.FastForwardBy(kEpsilon);
    LaunchApp();
    testing::Mock::VerifyAndClearExpectations(service());
  }

  webauthn::MockDeviceAuthorizationService* service() {
    return static_cast<webauthn::MockDeviceAuthorizationService*>(
        IOSDeviceAuthorizationServiceFactory::GetForProfile(profile_.get()));
  }

  // Returns whether a failed fetch is recorded in the backoff prefs.
  bool IsBackoffRecorded() {
    PrefService* pref_service = profile_->GetPrefs();
    return pref_service->HasPrefPath(
               prefs::kDeviceAuthorizationStartupFetchNextTime) ||
           pref_service->HasPrefPath(
               prefs::kDeviceAuthorizationStartupFetchBackoff);
  }

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* profile_state_;
  DeviceAuthorizationProfileAgent* agent_;
};

// Tests that keys are fetched on launch when signed in, and not before the
// profile reaches `kFinal`.
TEST_F(DeviceAuthorizationProfileAgentTest, FetchesOnLaunch) {
  SignIn(kEmail);
  AttachAgent();

  EXPECT_CALL(*service(), GetOrFetchKeys).Times(0);
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kNormalUI);
  RunDeferredFetch();
  testing::Mock::VerifyAndClearExpectations(service());

  EXPECT_CALL(*service(), GetOrFetchKeys);
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
  RunDeferredFetch();
}

// Tests that keys are fetched on every launch after a successful fetch, which
// is cheap since the service returns valid cached keys without a network fetch.
TEST_F(DeviceAuthorizationProfileAgentTest, DoesNotBackOffAfterSuccess) {
  SignIn(kEmail);
  EXPECT_CALL(*service(), GetOrFetchKeys)
      .Times(2)
      .WillRepeatedly(RunOnceCallbackRepeatedly<0>(SuccessResult()));
  LaunchApp();
  LaunchApp();
  EXPECT_FALSE(IsBackoffRecorded());
}

// Tests that keys are not fetched when signed out.
TEST_F(DeviceAuthorizationProfileAgentTest, DoesNotFetchWhenSignedOut) {
  EXPECT_CALL(*service(), GetOrFetchKeys).Times(0);
  LaunchApp();
}

// Tests that keys are fetched when a primary account is set after launch.
TEST_F(DeviceAuthorizationProfileAgentTest, FetchesOnSignIn) {
  LaunchApp();
  EXPECT_CALL(*service(), GetOrFetchKeys);
  SignIn(kEmail);
  RunDeferredFetch();
}

// Tests that keys are not fetched, nor attempts recorded, when the silent fetch
// feature is disabled.
TEST_F(DeviceAuthorizationProfileAgentTest, DoesNotFetchWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      webauthn::features::kDeviceAuthorizationStartupSilentFetch);

  SignIn(kEmail);
  EXPECT_CALL(*service(), GetOrFetchKeys).Times(0);
  LaunchApp();
  EXPECT_FALSE(IsBackoffRecorded());
}

// Tests that failed fetches back off exponentially up to the maximum.
TEST_F(DeviceAuthorizationProfileAgentTest, BacksOffAfterFailures) {
  SignIn(kEmail);
  EXPECT_CALL(*service(), GetOrFetchKeys);
  LaunchApp();
  testing::Mock::VerifyAndClearExpectations(service());

  for (const base::TimeDelta backoff :
       {kDeviceAuthorizationInitialBackoff, base::Hours(2), base::Hours(4),
        base::Hours(8), base::Hours(16), base::Hours(32), base::Hours(64),
        base::Hours(128), kDeviceAuthorizationMaximumBackoff,
        kDeviceAuthorizationMaximumBackoff}) {
    SCOPED_TRACE(testing::Message() << "Backoff: " << backoff);
    ExpectNextFetchDueAfter(backoff);
  }
}

// Tests that fetches requiring ReAuth back off like failures, since ReAuth is
// not actionable without UI.
TEST_F(DeviceAuthorizationProfileAgentTest, BacksOffWhenReAuthRequired) {
  SignIn(kEmail);
  EXPECT_CALL(*service(), GetOrFetchKeys)
      .WillOnce(RunOnceCallback<0>(ReAuthRequiredResult()));
  LaunchApp();
  ExpectNextFetchDueAfter(kDeviceAuthorizationInitialBackoff);
}

// Tests that a successful fetch resets the backoff, so that it restarts from
// the initial backoff after the next failure.
TEST_F(DeviceAuthorizationProfileAgentTest, ResetsBackoffOnSuccess) {
  SignIn(kEmail);
  EXPECT_CALL(*service(), GetOrFetchKeys);
  LaunchApp();
  testing::Mock::VerifyAndClearExpectations(service());

  ON_CALL(*service(), GetOrFetchKeys)
      .WillByDefault(RunOnceCallbackRepeatedly<0>(SuccessResult()));
  ExpectNextFetchDueAfter(kDeviceAuthorizationInitialBackoff);
  EXPECT_FALSE(IsBackoffRecorded());

  ON_CALL(*service(), GetOrFetchKeys)
      .WillByDefault(RunOnceCallbackRepeatedly<0>(ErrorResult()));
  EXPECT_CALL(*service(), GetOrFetchKeys);
  LaunchApp();
  testing::Mock::VerifyAndClearExpectations(service());
  ExpectNextFetchDueAfter(kDeviceAuthorizationInitialBackoff);
}

// Tests that the backoff restarts whenever the primary account changes, so that
// a newly signed-in account is fetched right away, including when switching
// back to a previous account.
TEST_F(DeviceAuthorizationProfileAgentTest, ResetsBackoffOnAccountChange) {
  SignIn(kEmail);
  EXPECT_CALL(*service(), GetOrFetchKeys);
  LaunchApp();
  testing::Mock::VerifyAndClearExpectations(service());

  EXPECT_CALL(*service(), GetOrFetchKeys);
  SignIn(kOtherEmail);
  RunDeferredFetch();
  testing::Mock::VerifyAndClearExpectations(service());

  EXPECT_CALL(*service(), GetOrFetchKeys);
  SignIn(kEmail);
  RunDeferredFetch();
}

// Tests that signing out resets the backoff, so that no state is kept for
// signed-out accounts, including accounts removed from the device.
TEST_F(DeviceAuthorizationProfileAgentTest, ResetsBackoffOnSignOut) {
  SignIn(kEmail);
  EXPECT_CALL(*service(), GetOrFetchKeys);
  LaunchApp();
  ASSERT_TRUE(IsBackoffRecorded());

  signin::ClearPrimaryAccount(identity_manager());
  EXPECT_FALSE(IsBackoffRecorded());
}

// Tests that a next fetch time beyond the maximum backoff, e.g. after the clock
// was moved backwards, doesn't block fetches.
TEST_F(DeviceAuthorizationProfileAgentTest, ClockMovedBackwards) {
  SignIn(kEmail);
  profile_->GetPrefs()->SetTime(
      prefs::kDeviceAuthorizationStartupFetchNextTime,
      base::Time::Now() + kDeviceAuthorizationMaximumBackoff + kEpsilon);

  EXPECT_CALL(*service(), GetOrFetchKeys);
  LaunchApp();
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_precondition_checker.h"

#import "base/files/file.h"
#import "base/memory/raw_ptr.h"
#import "components/signin/public/base/signin_metrics.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class RemoteActorPreconditionCheckerTest : public PlatformTest {
 protected:
  RemoteActorPreconditionCheckerTest() {
    base::File::Error file_error;
    FirstRun::RemoveSentinel();
    FirstRun::CreateSentinel(&file_error);
    FirstRun::LoadSentinelInfo();
    FirstRun::ClearStateForTesting();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              SyncServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_);
  }

  void TearDown() override {
    authentication_service_ = nullptr;
    profile_ = nullptr;
    FirstRun::RemoveSentinel();
    FirstRun::ClearStateForTesting();
    PlatformTest::TearDown();
  }

  FakeSystemIdentity* AddAndSignInFakeIdentity() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager())
        ->AddIdentity(identity);
    authentication_service_->SignIn(identity,
                                    signin_metrics::AccessPoint::kStartPage);
    return identity;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;
  raw_ptr<AuthenticationService> authentication_service_;
};

// Tests that preconditions succeed when FRE is complete, the user is signed
// in, and the requested GAIA ID matches the active profile's GAIA ID.
TEST_F(RemoteActorPreconditionCheckerTest,
       SuccessWhenSignedInAndGaiaIDMatches) {
  FakeSystemIdentity* identity = AddAndSignInFakeIdentity();

  auto result =
      CheckRemoteActorPreconditions(profile_, identity.gaiaId.ToString());
  EXPECT_TRUE(result.has_value());
}

// Tests that signed-out state fails with kNotSignedIn and maps to
// CANCELLED / NOT_SIGNED_IN.
TEST_F(RemoteActorPreconditionCheckerTest, FailsWithNotSignedInWhenSignedOut) {
  auto result = CheckRemoteActorPreconditions(profile_, "some_gaia_id");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorPreconditionError::kNotSignedIn);

  auto params = GetPreconditionFailureCallbackParams(result.error());
  EXPECT_EQ(params.status, RemoteActorCallbackStatus::kCancelled);
  EXPECT_EQ(params.reason, RemoteActorCallbackReason::kNotSignedIn);
}

// Tests that a null profile fails with kNotSignedIn.
TEST_F(RemoteActorPreconditionCheckerTest,
       FailsWithNotSignedInWhenProfileIsNull) {
  auto result = CheckRemoteActorPreconditions(/*profile=*/nullptr, "some_gaia");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorPreconditionError::kNotSignedIn);
}

// Tests that incomplete FRE fails with kFirstRunIncomplete even when signed in
// with a matching GAIA ID, and maps to CANCELLED / NOT_SIGNED_IN.
TEST_F(RemoteActorPreconditionCheckerTest,
       FailsWithFirstRunIncompleteWhenFREPending) {
  FakeSystemIdentity* identity = AddAndSignInFakeIdentity();
  FirstRun::RemoveSentinel();
  FirstRun::ClearStateForTesting();

  auto result =
      CheckRemoteActorPreconditions(profile_, identity.gaiaId.ToString());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorPreconditionError::kFirstRunIncomplete);

  auto params = GetPreconditionFailureCallbackParams(result.error());
  EXPECT_EQ(params.status, RemoteActorCallbackStatus::kCancelled);
  EXPECT_EQ(params.reason, RemoteActorCallbackReason::kNotSignedIn);
}

// Tests that a mismatched GAIA ID fails with kProfileMismatch and maps to
// FAILED / PROFILE_MISMATCH.
TEST_F(RemoteActorPreconditionCheckerTest,
       FailsWithProfileMismatchWhenGaiaIDDiffers) {
  AddAndSignInFakeIdentity();

  auto result = CheckRemoteActorPreconditions(profile_, "different_gaia_id");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorPreconditionError::kProfileMismatch);

  auto params = GetPreconditionFailureCallbackParams(result.error());
  EXPECT_EQ(params.status, RemoteActorCallbackStatus::kFailed);
  EXPECT_EQ(params.reason, RemoteActorCallbackReason::kProfileMismatch);
}

// Tests that an empty requested GAIA ID fails with kProfileMismatch.
TEST_F(RemoteActorPreconditionCheckerTest,
       FailsWithProfileMismatchWhenRequestedGaiaIDIsEmpty) {
  AddAndSignInFakeIdentity();

  auto result = CheckRemoteActorPreconditions(profile_, "");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RemoteActorPreconditionError::kProfileMismatch);
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_test_util.h"

#import "ios/chrome/browser/shared/model/profile/profile_ios_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class SigninTestUtilTest : public PlatformTest {
 protected:
  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
};

// Tests that `AddManagedProfileWithBuilder()` creates a non-personal signed-in
// profile where `HasPrimaryIdentityManaged()` returns `true`.
TEST_F(SigninTestUtilTest, CreatesSignedInManagedProfile) {
  FakeSystemIdentity* managed_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  TestProfileIOS* managed_profile =
      AddManagedProfileWithBuilder(profile_manager_, managed_identity);

  ASSERT_TRUE(managed_profile);
  EXPECT_FALSE(IsPersonalProfile(managed_profile));
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(managed_profile);
  EXPECT_TRUE(authentication_service->HasPrimaryIdentity());
  EXPECT_TRUE(authentication_service->HasPrimaryIdentityManaged());
}

}  // namespace

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"

#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using TestProfileManagerIOSTest = PlatformTest;

// Tests that the list of loaded profiles is empty after construction.
TEST_F(TestProfileManagerIOSTest, Constructor) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestProfileManagerIOS profile_manager;
  EXPECT_EQ(0U, profile_manager.GetLoadedProfiles().size());
}

// Tests that the list of loaded profiles has one element after calling
// `AddProfileWithBuilder(...)` and that it is marked as the personal profile.
TEST_F(TestProfileManagerIOSTest, AddProfileWithBuilder) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestProfileManagerIOS profile_manager;
  TestProfileIOS* personal_profile =
      profile_manager.AddProfileWithBuilder(TestProfileIOS::Builder());
  ASSERT_TRUE(personal_profile);
  EXPECT_EQ(1U, profile_manager.GetLoadedProfiles().size());
  EXPECT_EQ(
      personal_profile->GetProfileName(),
      profile_manager.GetProfileAttributesStorage()->GetPersonalProfileName());
}

// Tests that `AddManagedProfileForGaiaIDWithBuilder(...)` builds the profile
// assigned to the managed identity and does not mark it as the personal
// profile.
TEST_F(TestProfileManagerIOSTest, AddManagedProfileForGaiaIDWithBuilder) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestProfileManagerIOS profile_manager;
  profile_manager.AddProfileWithBuilder(TestProfileIOS::Builder());

  FakeSystemIdentity* identity = [FakeSystemIdentity fakeManagedIdentity];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(identity);

  TestProfileIOS* managed_profile =
      profile_manager.AddManagedProfileForGaiaIDWithBuilder(
          identity.gaiaId, TestProfileIOS::Builder());
  ASSERT_TRUE(managed_profile);
  EXPECT_EQ(2U, profile_manager.GetLoadedProfiles().size());
  EXPECT_NE(
      managed_profile->GetProfileName(),
      profile_manager.GetProfileAttributesStorage()->GetPersonalProfileName());
  EXPECT_EQ(managed_profile->GetProfileName(),
            GetApplicationContext()
                ->GetAccountProfileMapper()
                ->FindProfileNameForGaiaID(identity.gaiaId));
}

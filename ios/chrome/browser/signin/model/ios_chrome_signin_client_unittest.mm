// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/ios_chrome_signin_client.h"

#import "base/functional/bind.h"
#import "components/signin/public/base/signin_client.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/signin_client_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using IOSChromeSigninClientTest = PlatformTest;

TEST_F(IOSChromeSigninClientTest, IsAccountSignedInOnOtherProfile) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestProfileManagerIOS profile_manager;

  TestProfileIOS::Builder builder1;
  builder1.SetName("Profile1");
  TestProfileIOS* profile1 =
      profile_manager.AddProfileWithBuilder(std::move(builder1));

  TestProfileIOS::Builder builder2;
  builder2.SetName("Profile2");
  TestProfileIOS* profile2 =
      profile_manager.AddProfileWithBuilder(std::move(builder2));

  const GaiaId gaia1("gaia_1");
  const GaiaId gaia2("gaia_2");
  const GaiaId gaia_unassigned("gaia_unassigned");

  ProfileAttributesStorageIOS* storage =
      profile_manager.GetProfileAttributesStorage();
  storage->UpdateAttributesForProfileWithName(
      profile1->GetProfileName(),
      base::BindOnce(
          [](const GaiaId& gaia_id, ProfileAttributesIOS& attrs) {
            attrs.SetAuthenticationInfo(gaia_id, "user1@gmail.com");
          },
          gaia1));
  storage->UpdateAttributesForProfileWithName(
      profile2->GetProfileName(),
      base::BindOnce(
          [](const GaiaId& gaia_id, ProfileAttributesIOS& attrs) {
            attrs.SetAuthenticationInfo(gaia_id, "user2@gmail.com");
          },
          gaia2));

  SigninClient* client1 = SigninClientFactory::GetForProfile(profile1);
  ASSERT_TRUE(client1);

  // Signed in on the current profile -> false.
  EXPECT_FALSE(client1->IsAccountSignedInOnOtherProfile(gaia1));
  // Signed in on another profile -> true.
  EXPECT_TRUE(client1->IsAccountSignedInOnOtherProfile(gaia2));
  // Not signed in on any profile -> false.
  EXPECT_FALSE(client1->IsAccountSignedInOnOtherProfile(gaia_unassigned));
  // Empty GaiaId -> false.
  EXPECT_FALSE(client1->IsAccountSignedInOnOtherProfile(GaiaId()));
}

}  // namespace

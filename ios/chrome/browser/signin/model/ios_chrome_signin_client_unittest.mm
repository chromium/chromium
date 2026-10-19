// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/ios_chrome_signin_client.h"

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_client.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/signin_client_factory.h"
#import "ios/chrome/browser/signin/model/signin_test_util.h"
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

  FakeSystemIdentity* personal_identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* managed_identity =
      [FakeSystemIdentity fakeManagedIdentity];

  TestProfileIOS* personal_profile =
      profile_manager.AddProfileWithBuilder(TestProfileIOS::Builder());
  TestProfileIOS* managed_profile =
      AddManagedProfileWithBuilder(profile_manager, managed_identity);

  const GaiaId gaia_unassigned("gaia_unassigned");

  ProfileAttributesStorageIOS* storage =
      profile_manager.GetProfileAttributesStorage();
  storage->UpdateAttributesForProfileWithName(
      personal_profile->GetProfileName(),
      base::BindOnce(
          [](FakeSystemIdentity* identity, ProfileAttributesIOS& attrs) {
            attrs.SetAuthenticationInfo(
                identity.gaiaId, base::SysNSStringToUTF8(identity.userEmail));
          },
          personal_identity));
  storage->UpdateAttributesForProfileWithName(
      managed_profile->GetProfileName(),
      base::BindOnce(
          [](FakeSystemIdentity* identity, ProfileAttributesIOS& attrs) {
            attrs.SetAuthenticationInfo(
                identity.gaiaId, base::SysNSStringToUTF8(identity.userEmail));
          },
          managed_identity));

  SigninClient* personal_client =
      SigninClientFactory::GetForProfile(personal_profile);
  ASSERT_TRUE(personal_client);

  // Signed in on the current profile -> false.
  EXPECT_FALSE(personal_client->IsAccountSignedInOnOtherProfile(
      personal_identity.gaiaId));
  // Signed in on another profile -> true.
  EXPECT_TRUE(personal_client->IsAccountSignedInOnOtherProfile(
      managed_identity.gaiaId));
  // Not signed in on any profile -> false.
  EXPECT_FALSE(
      personal_client->IsAccountSignedInOnOtherProfile(gaia_unassigned));
  // Empty GaiaId -> false.
  EXPECT_FALSE(personal_client->IsAccountSignedInOnOtherProfile(GaiaId()));
}

}  // namespace

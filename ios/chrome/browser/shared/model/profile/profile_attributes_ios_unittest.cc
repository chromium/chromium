// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"

#include "base/time/time.h"
#include "testing/platform_test.h"

namespace {

// Constants used by tests.
constexpr char kProfileName[] = "Profile";
constexpr GaiaId::Literal kGaiaId1("Gaia1");
constexpr GaiaId::Literal kGaiaId2("Gaia2");
constexpr char kUserName[] = "email@example.com";

constexpr char kFakeNotificationClient1[] = "CLIENT_1";
constexpr char kFakeNotificationClient2[] = "CLIENT_2";

}  // namespace

using ProfileAttributesIOSTest = PlatformTest;

// Tests the CreateNew() factory of ProfileAttributesIOS.
TEST_F(ProfileAttributesIOSTest, CreateNew) {
  ProfileAttributesIOS attributes =
      ProfileAttributesIOS::CreateNew(kProfileName);
  EXPECT_EQ(attributes.GetProfileName(), kProfileName);
  EXPECT_TRUE(attributes.IsNewProfile());

  attributes.ClearIsNewProfile();
  EXPECT_FALSE(attributes.IsNewProfile());
}

// Tests that IsFullyInitialized() starts out false and can be set to true.
TEST_F(ProfileAttributesIOSTest, FullyInitialized) {
  ProfileAttributesIOS attributes =
      ProfileAttributesIOS::CreateNew(kProfileName);
  EXPECT_EQ(attributes.GetProfileName(), kProfileName);
  EXPECT_FALSE(attributes.IsFullyInitialized());

  attributes.SetFullyInitialized();
  EXPECT_TRUE(attributes.IsFullyInitialized());
}

// Tests the GetName() method of ProfileAttributesIOS.
TEST_F(ProfileAttributesIOSTest, GetName) {
  ProfileAttributesIOS attributes =
      ProfileAttributesIOS::WithAttrs(kProfileName, base::Value::Dict());
  EXPECT_EQ(attributes.GetProfileName(), kProfileName);
}

// Tests that setting and reading the authentication info works.
TEST_F(ProfileAttributesIOSTest, GetSetAuthenticationInfo) {
  ProfileAttributesIOS attributes =
      ProfileAttributesIOS::WithAttrs(kProfileName, base::Value::Dict());
  EXPECT_EQ(attributes.GetGaiaId(), GaiaId());
  EXPECT_EQ(attributes.GetUserName(), "");
  EXPECT_FALSE(attributes.HasAuthenticationError());
  EXPECT_FALSE(attributes.IsAuthenticated());

  attributes.SetAuthenticationInfo(kGaiaId1, kUserName);
  EXPECT_EQ(attributes.GetGaiaId(), kGaiaId1);
  EXPECT_EQ(attributes.GetUserName(), kUserName);
  EXPECT_FALSE(attributes.HasAuthenticationError());
  EXPECT_TRUE(attributes.IsAuthenticated());

  attributes.SetHasAuthenticationError(true);
  EXPECT_TRUE(attributes.HasAuthenticationError());
}

// Tests that setting and reading the attached gaia ids.
TEST_F(ProfileAttributesIOSTest, GetSetAttachedGaiaIds) {
  ProfileAttributesIOS attributes =
      ProfileAttributesIOS::WithAttrs(kProfileName, base::Value::Dict());

  EXPECT_EQ(attributes.GetAttachedGaiaIds().size(), 0ul);
  ProfileAttributesIOS::GaiaIdSet gaia_ids = {kGaiaId1, kGaiaId2};
  ASSERT_EQ(gaia_ids.size(), 2u);
  attributes.SetAttachedGaiaIds(gaia_ids);
  EXPECT_EQ(attributes.GetAttachedGaiaIds(), gaia_ids);
}

// Tests that setting and reading the last activation time works.
TEST_F(ProfileAttributesIOSTest, GetSetLastActiveTime) {
  ProfileAttributesIOS attributes =
      ProfileAttributesIOS::WithAttrs(kProfileName, base::Value::Dict());

  const base::Time now = base::Time::Now();
  EXPECT_NE(attributes.GetLastActiveTime(), now);

  attributes.SetLastActiveTime(now);
  EXPECT_EQ(attributes.GetLastActiveTime(), now);
}

// Tests that the internal storage can be accessed.
TEST_F(ProfileAttributesIOSTest, GetStorage) {
  {
    ProfileAttributesIOS attributes =
        ProfileAttributesIOS::WithAttrs(kProfileName, base::Value::Dict());
    EXPECT_EQ(std::move(attributes).GetStorage(), base::Value::Dict());
  }

  {
    ProfileAttributesIOS attributes =
        ProfileAttributesIOS::WithAttrs(kProfileName, base::Value::Dict());
    attributes.SetAuthenticationInfo(kGaiaId1, kUserName);
    attributes.SetLastActiveTime(base::Time::Now());
    attributes.SetHasAuthenticationError(true);
    EXPECT_EQ(std::move(attributes).GetStorage().size(), 4u);
  }
}

// Tests setting and reading the notification permissions.
TEST_F(ProfileAttributesIOSTest, GetNotificationPermissions) {
  ProfileAttributesIOS attributes =
      ProfileAttributesIOS::WithAttrs(kProfileName, base::Value::Dict());
  EXPECT_EQ(attributes.GetNotificationPermissions(), nullptr);

  base::Value::Dict permissions;
  permissions.Set(kFakeNotificationClient1, true);
  permissions.Set(kFakeNotificationClient2, false);
  attributes.SetNotificationPermissions(permissions.Clone());
  EXPECT_NE(attributes.GetNotificationPermissions(), nullptr);
  EXPECT_EQ(attributes.GetNotificationPermissions()->FindBool(
                kFakeNotificationClient1),
            true);
  EXPECT_EQ(attributes.GetNotificationPermissions()->FindBool(
                kFakeNotificationClient2),
            false);
}

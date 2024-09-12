// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"

#include "base/time/time.h"
#include "testing/platform_test.h"

namespace {

// Constants used by tests.
const char kProfileName[] = "Profile";
const char kGaiaId1[] = "Gaia1";
const char kGaiaId2[] = "Gaia2";
const char kUserName[] = "email@example.com";

}  // namespace

using ProfileAttributesIOSTest = PlatformTest;

// Tests the GetName() method of ProfileAttributesIOS.
TEST_F(ProfileAttributesIOSTest, GetName) {
  ProfileAttributesIOS attributes(kProfileName, /*attrs=*/nullptr);
  EXPECT_EQ(attributes.GetProfileName(), kProfileName);
}

// Tests that setting and reading the authentication info works.
TEST_F(ProfileAttributesIOSTest, GetSetAuthenticationInfo) {
  ProfileAttributesIOS attributes(kProfileName, /*attrs=*/nullptr);
  EXPECT_EQ(attributes.GetGaiaId(), "");
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
  ProfileAttributesIOS attributes(kProfileName, /*attrs=*/nullptr);

  EXPECT_EQ(attributes.GetAttachedGaiaIds().size(), 0ul);
  ProfileAttributesIOS::GaiaIdSet gaia_ids;
  gaia_ids.insert(kGaiaId1);
  gaia_ids.insert(kGaiaId2);
  ASSERT_EQ(gaia_ids.size(), 2u);
  attributes.SetAttachedGaiaIds(gaia_ids);
  EXPECT_EQ(attributes.GetAttachedGaiaIds(), gaia_ids);
}

// Tests that setting and reading the last activation time works.
TEST_F(ProfileAttributesIOSTest, GetSetLastActiveTime) {
  ProfileAttributesIOS attributes(kProfileName, /*attrs=*/nullptr);

  const base::Time now = base::Time::Now();
  EXPECT_NE(attributes.GetLastActiveTime(), now);

  attributes.SetLastActiveTime(now);
  EXPECT_EQ(attributes.GetLastActiveTime(), now);
}

// Tests that the internal storage can be accessed.
TEST_F(ProfileAttributesIOSTest, GetStorage) {
  {
    ProfileAttributesIOS attributes(kProfileName, /*attrs=*/nullptr);
    EXPECT_EQ(std::move(attributes).GetStorage(), base::Value::Dict());
  }

  {
    ProfileAttributesIOS attributes(kProfileName, /*attrs=*/nullptr);
    attributes.SetAuthenticationInfo(kGaiaId1, kUserName);
    attributes.SetLastActiveTime(base::Time::Now());
    attributes.SetHasAuthenticationError(false);
    EXPECT_EQ(std::move(attributes).GetStorage().size(), 4u);
  }
}

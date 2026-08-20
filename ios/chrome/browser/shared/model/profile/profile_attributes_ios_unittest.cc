// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"

#include <string_view>

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

namespace {

// Constants used by tests.
constexpr std::string_view kProfileName = "Profile";
constexpr GaiaId::Literal kGaiaId1("Gaia1");
constexpr GaiaId::Literal kGaiaId2("Gaia2");
constexpr std::string_view kUserName = "email@example.com";

constexpr std::string_view kFakeNotificationClient1 = "CLIENT_1";
constexpr std::string_view kFakeNotificationClient2 = "CLIENT_2";

constexpr std::string_view kSession1 = "Session1";
constexpr std::string_view kSession2 = "Session2";

constexpr std::string_view kTimePref = "Time";
constexpr std::string_view kBoolPref = "Bool";

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
      ProfileAttributesIOS::WithAttrs(kProfileName, base::DictValue());
  EXPECT_EQ(attributes.GetProfileName(), kProfileName);
}

// Tests that setting and reading the authentication info works.
TEST_F(ProfileAttributesIOSTest, GetSetAuthenticationInfo) {
  ProfileAttributesIOS attributes =
      ProfileAttributesIOS::WithAttrs(kProfileName, base::DictValue());
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
      ProfileAttributesIOS::WithAttrs(kProfileName, base::DictValue());

  EXPECT_EQ(attributes.GetAttachedGaiaIds().size(), 0ul);
  ProfileAttributesIOS::GaiaIdSet gaia_ids = {kGaiaId1, kGaiaId2};
  ASSERT_EQ(gaia_ids.size(), 2u);
  attributes.SetAttachedGaiaIds(gaia_ids);
  EXPECT_EQ(attributes.GetAttachedGaiaIds(), gaia_ids);
}

// Tests that setting and reading the last activation time works.
TEST_F(ProfileAttributesIOSTest, GetSetLastActiveTime) {
  ProfileAttributesIOS attributes =
      ProfileAttributesIOS::WithAttrs(kProfileName, base::DictValue());

  const base::Time now = base::Time::Now();
  EXPECT_NE(attributes.GetLastActiveTime(), now);

  attributes.SetLastActiveTime(now);
  EXPECT_EQ(attributes.GetLastActiveTime(), now);
}

// Tests that the internal storage can be accessed.
TEST_F(ProfileAttributesIOSTest, GetStorage) {
  {
    ProfileAttributesIOS attributes =
        ProfileAttributesIOS::WithAttrs(kProfileName, base::DictValue());
    EXPECT_EQ(std::move(attributes).GetStorage(), base::DictValue());
  }

  {
    ProfileAttributesIOS attributes =
        ProfileAttributesIOS::WithAttrs(kProfileName, base::DictValue());
    attributes.SetAuthenticationInfo(kGaiaId1, kUserName);
    attributes.SetLastActiveTime(base::Time::Now());
    attributes.SetHasAuthenticationError(true);
    EXPECT_EQ(std::move(attributes).GetStorage().size(), 4u);
  }
}

// Tests setting and reading the notification permissions.
TEST_F(ProfileAttributesIOSTest, GetNotificationPermissions) {
  ProfileAttributesIOS attributes =
      ProfileAttributesIOS::WithAttrs(kProfileName, base::DictValue());
  EXPECT_EQ(attributes.GetNotificationPermissions(), nullptr);

  base::DictValue permissions;
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

// Tests setting and reading the session scoped preferences.
TEST_F(ProfileAttributesIOSTest, SessionScopedPreferences) {
  ProfileAttributesIOS attributes =
      ProfileAttributesIOS::WithAttrs(kProfileName, base::DictValue());

  const base::Time never;
  EXPECT_EQ(attributes.HasSessionScopedPrefs(kSession1), false);
  EXPECT_EQ(attributes.HasSessionScopedPrefs(kSession2), false);
  EXPECT_EQ(attributes.GetSessionScopedTimePref(kSession1, kTimePref), never);
  EXPECT_EQ(attributes.GetSessionScopedTimePref(kSession2, kTimePref), never);
  EXPECT_EQ(attributes.GetSessionScopedBoolPref(kSession1, kBoolPref), false);
  EXPECT_EQ(attributes.GetSessionScopedBoolPref(kSession2, kBoolPref), false);

  const base::Time now = base::Time::Now();
  attributes.SetSessionScopedTimePref(kSession1, kTimePref, now);
  EXPECT_EQ(attributes.HasSessionScopedPrefs(kSession1), true);
  EXPECT_EQ(attributes.HasSessionScopedPrefs(kSession2), false);
  EXPECT_EQ(attributes.GetSessionScopedTimePref(kSession1, kTimePref), now);
  EXPECT_EQ(attributes.GetSessionScopedTimePref(kSession2, kTimePref), never);
  EXPECT_EQ(attributes.GetSessionScopedBoolPref(kSession1, kBoolPref), false);
  EXPECT_EQ(attributes.GetSessionScopedBoolPref(kSession2, kBoolPref), false);

  attributes.SetSessionScopedBoolPref(kSession2, kBoolPref, true);
  EXPECT_EQ(attributes.HasSessionScopedPrefs(kSession1), true);
  EXPECT_EQ(attributes.HasSessionScopedPrefs(kSession2), true);
  EXPECT_EQ(attributes.GetSessionScopedTimePref(kSession1, kTimePref), now);
  EXPECT_EQ(attributes.GetSessionScopedTimePref(kSession2, kTimePref), never);
  EXPECT_EQ(attributes.GetSessionScopedBoolPref(kSession1, kBoolPref), false);
  EXPECT_EQ(attributes.GetSessionScopedBoolPref(kSession2, kBoolPref), true);
}

// Tests clearing the session scoped preferences.
TEST_F(ProfileAttributesIOSTest, ClearSessionScopedPreferences) {
  ProfileAttributesIOS attributes =
      ProfileAttributesIOS::WithAttrs(kProfileName, base::DictValue());

  const base::Time now = base::Time::Now();
  attributes.SetSessionScopedTimePref(kSession1, kTimePref, now);
  attributes.SetSessionScopedTimePref(kSession2, kTimePref, now);
  attributes.SetSessionScopedBoolPref(kSession1, kBoolPref, true);
  attributes.SetSessionScopedBoolPref(kSession2, kBoolPref, true);

  EXPECT_EQ(attributes.HasSessionScopedPrefs(kSession1), true);
  EXPECT_EQ(attributes.HasSessionScopedPrefs(kSession2), true);
  EXPECT_EQ(attributes.GetSessionScopedTimePref(kSession1, kTimePref), now);
  EXPECT_EQ(attributes.GetSessionScopedTimePref(kSession2, kTimePref), now);
  EXPECT_EQ(attributes.GetSessionScopedBoolPref(kSession1, kBoolPref), true);
  EXPECT_EQ(attributes.GetSessionScopedBoolPref(kSession2, kBoolPref), true);

  attributes.ClearSessionScopedPrefs(kSession2);

  const base::Time never;
  EXPECT_EQ(attributes.HasSessionScopedPrefs(kSession1), true);
  EXPECT_EQ(attributes.HasSessionScopedPrefs(kSession2), false);
  EXPECT_EQ(attributes.GetSessionScopedTimePref(kSession1, kTimePref), now);
  EXPECT_EQ(attributes.GetSessionScopedTimePref(kSession2, kTimePref), never);
  EXPECT_EQ(attributes.GetSessionScopedBoolPref(kSession1, kBoolPref), true);
  EXPECT_EQ(attributes.GetSessionScopedBoolPref(kSession2, kBoolPref), false);
}

// Tests retrieving the identifier of all known sessions.
TEST_F(ProfileAttributesIOSTest, GetKnownSessions) {
  ProfileAttributesIOS attributes =
      ProfileAttributesIOS::WithAttrs(kProfileName, base::DictValue());

  // Check that no sessions are known for a newly created instance.
  EXPECT_THAT(attributes.GetKnownSessions(), IsEmpty());

  // Check that setting non-default preferences causes the session identifiers
  // to be returned by GetKnownSessions().
  attributes.SetSessionScopedTimePref(kSession1, kTimePref, base::Time::Now());
  attributes.SetSessionScopedBoolPref(kSession2, kBoolPref, true);
  EXPECT_THAT(attributes.GetKnownSessions(),
              UnorderedElementsAre(kSession1, kSession2));

  // Check that clearing the preferences for a session remove it from the
  // set of sessions returned by GetKnownSessions().
  attributes.ClearSessionScopedPrefs(kSession2);
  EXPECT_THAT(attributes.GetKnownSessions(), UnorderedElementsAre(kSession1));

  // Check that setting a preference to a default value causes the session
  // to be added to the known set, even though ProfileAttributesIOS does
  // not store the value.
  attributes.SetSessionScopedBoolPref(kSession2, kBoolPref, false);
  EXPECT_THAT(attributes.GetKnownSessions(),
              UnorderedElementsAre(kSession1, kSession2));
}

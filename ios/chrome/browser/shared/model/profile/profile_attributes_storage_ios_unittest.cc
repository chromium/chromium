// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"

#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#include "testing/platform_test.h"

namespace {

// Constants used by tests.
struct TestAccount {
  std::string_view name;
  std::string_view gaia;
  std::string_view email;
  bool authenticated;
  base::Time last_active_time;
};

constexpr TestAccount kTestAccounts[] = {
    {
        .name = "Profile1",
        .gaia = "Gaia1",
        .email = "email1@example.com",
        .authenticated = true,
        .last_active_time = base::Time::UnixEpoch() + base::Minutes(1),
    },
    {
        .name = "Profile2",
        .gaia = "Gaia2",
        .email = "",
        .authenticated = true,
        .last_active_time = base::Time::UnixEpoch() + base::Minutes(2),
    },
    {
        .name = "Profile3",
        .gaia = "",
        .email = "email3@example.com",
        .authenticated = true,
        .last_active_time = base::Time::UnixEpoch() + base::Minutes(3),
    },
    {
        .name = "Profile4",
        .gaia = "",
        .email = "",
        .authenticated = false,
        .last_active_time = base::Time::UnixEpoch() + base::Minutes(4),
    },
};

constexpr char kTestProfile1[] = "Profile1";
constexpr char kTestProfile2[] = "Profile2";
constexpr char kTestSceneId1[] = "scene-id1";
constexpr char kTestSceneId2[] = "scene-id2";

}  // namespace

class ProfileAttributesStorageIOSTest : public PlatformTest {
 public:
  ProfileAttributesStorageIOSTest() {
    ProfileAttributesStorageIOS::RegisterPrefs(
        testing_pref_service_.registry());
  }

  PrefService* pref_service() { return &testing_pref_service_; }

 private:
  TestingPrefServiceSimple testing_pref_service_;
};

// Tests that AddProfile(...) inserts data for a Profile.
TEST_F(ProfileAttributesStorageIOSTest, AddProfile) {
  ProfileAttributesStorageIOS storage(pref_service());

  for (const TestAccount& account : kTestAccounts) {
    EXPECT_FALSE(storage.HasProfileWithName(account.name));

    storage.AddProfile(account.name);
    ASSERT_TRUE(storage.HasProfileWithName(account.name));

    ProfileAttributesIOS attr =
        storage.GetAttributesForProfileWithName(account.name);
    EXPECT_EQ(attr.GetProfileName(), account.name);
    EXPECT_EQ(std::move(attr).GetStorage(), base::Value::Dict());
  }

  // There is no duplicate, so there should be exactly as many Profile
  // known to the storage as there are test accounts.
  EXPECT_EQ(storage.GetNumberOfProfiles(), std::size(kTestAccounts));
}

// Tests that RemoveProfile(...) removes data for a Profile.
TEST_F(ProfileAttributesStorageIOSTest, RemoveProfile) {
  ProfileAttributesStorageIOS storage(pref_service());

  for (const TestAccount& account : kTestAccounts) {
    storage.AddProfile(account.name);
  }

  // There is no duplicate, so there should be exactly as many Profile
  // known to the storage as there are test accounts.
  EXPECT_EQ(storage.GetNumberOfProfiles(), std::size(kTestAccounts));

  for (const TestAccount& account : kTestAccounts) {
    EXPECT_TRUE(storage.HasProfileWithName(account.name));

    storage.RemoveProfile(account.name);

    EXPECT_FALSE(storage.HasProfileWithName(account.name));
  }
}

// Tests that the saved profile name can be retrieved with the scene ID.
TEST_F(ProfileAttributesStorageIOSTest, MapProfileAndSceneID) {
  ProfileAttributesStorageIOS storage(pref_service());

  storage.AddProfile(kTestProfile1);
  ASSERT_EQ(storage.GetProfileNameForSceneID(kTestSceneId1), std::string());
  ASSERT_EQ(storage.GetProfileNameForSceneID(kTestSceneId2), std::string());

  storage.SetProfileNameForSceneID(kTestSceneId1, kTestProfile1);
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId1), kTestProfile1);
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId2), std::string());

  storage.SetProfileNameForSceneID(kTestSceneId2, kTestProfile1);
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId1), kTestProfile1);
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId2), kTestProfile1);

  storage.ClearProfileNameForSceneID(kTestSceneId1);
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId1), std::string());
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId2), kTestProfile1);
}

// Tests that removing a profile orphans all attached scenes.
TEST_F(ProfileAttributesStorageIOSTest, RemoveProfileDisconnectScenes) {
  ProfileAttributesStorageIOS storage(pref_service());

  storage.AddProfile(kTestProfile1);
  storage.AddProfile(kTestProfile2);
  storage.SetProfileNameForSceneID(kTestSceneId1, kTestProfile1);
  storage.SetProfileNameForSceneID(kTestSceneId2, kTestProfile2);
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId1), kTestProfile1);
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId2), kTestProfile2);

  storage.RemoveProfile(kTestProfile1);
  EXPECT_FALSE(storage.HasProfileWithName(kTestProfile1));
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId1), std::string());
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId2), kTestProfile2);
}

// Tests that settings and getting the attributes using ProfileAttributesIOS
// works as expected, and they are correctly stored in the local preferences
// when committed.
// Note that this implicitly tests UpdateAttributesForProfileAtIndex(...).
TEST_F(ProfileAttributesStorageIOSTest, UpdateAttributesForProfileWithName) {
  {
    ProfileAttributesStorageIOS storage(pref_service());

    for (const TestAccount& account : kTestAccounts) {
      storage.AddProfile(account.name);

      storage.UpdateAttributesForProfileWithName(
          account.name,
          base::BindOnce(
              [](const TestAccount& account, ProfileAttributesIOS attr) {
                attr.SetLastActiveTime(account.last_active_time);
                attr.SetAuthenticationInfo(account.gaia, account.email);
                ProfileAttributesIOS::GaiaIdSet gaia_ids;
                gaia_ids.insert(std::string(account.gaia));
                attr.SetAttachedGaiaIds(gaia_ids);
                return attr;
              },
              account));
    }
  }

  ProfileAttributesStorageIOS storage(pref_service());
  for (const TestAccount& account : kTestAccounts) {
    ASSERT_TRUE(storage.HasProfileWithName(account.name));

    ProfileAttributesIOS attr =
        storage.GetAttributesForProfileWithName(account.name);
    EXPECT_EQ(attr.GetProfileName(), account.name);
    EXPECT_EQ(attr.GetGaiaId(), account.gaia);
    EXPECT_EQ(attr.GetUserName(), account.email);
    EXPECT_EQ(attr.IsAuthenticated(), account.authenticated);
    EXPECT_EQ(attr.GetLastActiveTime(), account.last_active_time);
    ProfileAttributesIOS::GaiaIdSet gaia_ids;
    gaia_ids.insert(std::string(account.gaia));
    attr.SetAttachedGaiaIds(gaia_ids);
    EXPECT_EQ(attr.GetAttachedGaiaIds(), gaia_ids);
  }
}

// Tests that GetAttributesForProfileWithName(...) works as expected.
// Note that this implicitly tests GetAttributesForProfileAtIndex(...).
TEST_F(ProfileAttributesStorageIOSTest, GetAttributesForProfileWithName) {
  ProfileAttributesStorageIOS storage(pref_service());

  for (const TestAccount& account : kTestAccounts) {
    storage.AddProfile(account.name);
    ProfileAttributesIOS attr =
        storage.GetAttributesForProfileWithName(account.name);

    EXPECT_EQ(attr.GetProfileName(), account.name);
    EXPECT_EQ(attr.GetGaiaId(), "");
    EXPECT_EQ(attr.GetUserName(), "");
    EXPECT_EQ(attr.IsAuthenticated(), false);
    EXPECT_EQ(attr.GetAttachedGaiaIds().size(), 0ul);
  }
}

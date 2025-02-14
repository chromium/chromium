// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/profile/mutable_profile_attributes_storage_ios.h"

#import "base/time/time.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"

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

constexpr TestAccount kDeletedAccount = {
    .name = "DeletedProfile",
    .gaia = "DeletedGaia",
    .email = "deleted@example.com",
    .last_active_time = base::Time::UnixEpoch() + base::Minutes(1),
};

constexpr char kTestProfile1[] = "Profile1";
constexpr char kTestProfile2[] = "Profile2";
constexpr char kTestSceneId1[] = "scene-id1";
constexpr char kTestSceneId2[] = "scene-id2";

// Updates `attr` with information from `account`.
void UpdateAttributesFromTestAccount(const TestAccount& account,
                                     ProfileAttributesIOS& attr) {
  CHECK_EQ(account.name, attr.GetProfileName());
  attr.SetLastActiveTime(account.last_active_time);
  attr.SetAuthenticationInfo(GaiaId(std::string(account.gaia)), account.email);
  ProfileAttributesIOS::GaiaIdSet gaia_ids;
  gaia_ids.insert(GaiaId(std::string(account.gaia)));
  attr.SetAttachedGaiaIds(gaia_ids);
}

}  // namespace

class MutableProfileAttributesStorageIOSTest : public PlatformTest {
 public:
  MutableProfileAttributesStorageIOSTest() {}

  PrefService* pref_service() {
    return GetApplicationContext()->GetLocalState();
  }

 private:
  IOSChromeScopedTestingLocalState scoped_local_state_;
};

// Tests that AddProfile(...) inserts data for a Profile.
TEST_F(MutableProfileAttributesStorageIOSTest, AddProfile) {
  MutableProfileAttributesStorageIOS storage(pref_service());

  for (const TestAccount& account : kTestAccounts) {
    EXPECT_FALSE(storage.HasProfileWithName(account.name));

    storage.AddProfile(account.name);
    ASSERT_TRUE(storage.HasProfileWithName(account.name));

    ProfileAttributesIOS attr =
        storage.GetAttributesForProfileWithName(account.name);
    EXPECT_EQ(attr.GetProfileName(), account.name);
    EXPECT_TRUE(attr.IsNewProfile());

    attr.ClearIsNewProfile();
    EXPECT_EQ(std::move(attr).GetStorage(), base::Value::Dict());
  }

  // There is no duplicate, so there should be exactly as many Profile
  // known to the storage as there are test accounts.
  EXPECT_EQ(storage.GetNumberOfProfiles(), std::size(kTestAccounts));
}

// Tests that MarkProfileForDeletion(...) removes data for a Profile.
TEST_F(MutableProfileAttributesStorageIOSTest, MarkProfileForDeletion) {
  MutableProfileAttributesStorageIOS storage(pref_service());

  for (const TestAccount& account : kTestAccounts) {
    storage.AddProfile(account.name);
  }

  // There is no duplicate, so there should be exactly as many Profile
  // known to the storage as there are test accounts.
  EXPECT_EQ(storage.GetNumberOfProfiles(), std::size(kTestAccounts));

  for (const TestAccount& account : kTestAccounts) {
    EXPECT_TRUE(storage.HasProfileWithName(account.name));

    storage.MarkProfileForDeletion(account.name);

    EXPECT_FALSE(storage.HasProfileWithName(account.name));
    EXPECT_TRUE(storage.IsProfileMarkedForDeletion(account.name));

    storage.ProfileDeletionComplete(account.name);
    EXPECT_FALSE(storage.HasProfileWithName(account.name));
    EXPECT_FALSE(storage.IsProfileMarkedForDeletion(account.name));
  }
}

// Tests that the saved profile name can be retrieved with the scene ID.
TEST_F(MutableProfileAttributesStorageIOSTest, MapProfileAndSceneID) {
  MutableProfileAttributesStorageIOS storage(pref_service());

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
TEST_F(MutableProfileAttributesStorageIOSTest, RemoveProfileDisconnectScenes) {
  MutableProfileAttributesStorageIOS storage(pref_service());

  storage.AddProfile(kTestProfile1);
  storage.AddProfile(kTestProfile2);
  storage.SetProfileNameForSceneID(kTestSceneId1, kTestProfile1);
  storage.SetProfileNameForSceneID(kTestSceneId2, kTestProfile2);
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId1), kTestProfile1);
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId2), kTestProfile2);

  storage.MarkProfileForDeletion(kTestProfile1);
  EXPECT_FALSE(storage.HasProfileWithName(kTestProfile1));
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId1), std::string());
  EXPECT_EQ(storage.GetProfileNameForSceneID(kTestSceneId2), kTestProfile2);
}

// Tests that settings and getting the attributes using ProfileAttributesIOS
// works as expected, and they are correctly stored in the local preferences
// when committed.
TEST_F(MutableProfileAttributesStorageIOSTest,
       UpdateAttributesForProfileWithName) {
  {
    MutableProfileAttributesStorageIOS storage(pref_service());

    for (const TestAccount& account : kTestAccounts) {
      storage.AddProfile(account.name);

      storage.UpdateAttributesForProfileWithName(
          account.name,
          base::BindOnce(&UpdateAttributesFromTestAccount, account));
    }
  }

  MutableProfileAttributesStorageIOS storage(pref_service());
  for (const TestAccount& account : kTestAccounts) {
    ASSERT_TRUE(storage.HasProfileWithName(account.name));

    ProfileAttributesIOS attr =
        storage.GetAttributesForProfileWithName(account.name);
    EXPECT_EQ(attr.GetProfileName(), account.name);
    EXPECT_EQ(attr.GetGaiaId(), GaiaId(std::string(account.gaia)));
    EXPECT_EQ(attr.GetUserName(), account.email);
    EXPECT_EQ(attr.IsAuthenticated(), account.authenticated);
    EXPECT_EQ(attr.GetLastActiveTime(), account.last_active_time);
    ProfileAttributesIOS::GaiaIdSet gaia_ids;
    gaia_ids.insert(GaiaId(std::string(account.gaia)));
    attr.SetAttachedGaiaIds(gaia_ids);
    EXPECT_EQ(attr.GetAttachedGaiaIds(), gaia_ids);
  }
}

// Tests that GetAttributesForProfileWithName(...) works as expected.
TEST_F(MutableProfileAttributesStorageIOSTest,
       GetAttributesForProfileWithName) {
  MutableProfileAttributesStorageIOS storage(pref_service());

  for (const TestAccount& account : kTestAccounts) {
    storage.AddProfile(account.name);
    ProfileAttributesIOS attr =
        storage.GetAttributesForProfileWithName(account.name);

    EXPECT_EQ(attr.GetProfileName(), account.name);
    EXPECT_EQ(attr.GetGaiaId(), GaiaId());
    EXPECT_EQ(attr.GetUserName(), "");
    EXPECT_EQ(attr.IsAuthenticated(), false);
    EXPECT_EQ(attr.GetAttachedGaiaIds().size(), 0ul);
  }
}

// Tests that GetAttributesForProfileWithName(...) returns an instance of
// ProfileAttributesIOS corresponding to a "deleted profile" when called
// with the name of a profile marked for deletion.
TEST_F(MutableProfileAttributesStorageIOSTest,
       GetAttributesForProfileWithName_DeletedProfile) {
  MutableProfileAttributesStorageIOS storage(pref_service());
  storage.AddProfile(kDeletedAccount.name);
  storage.UpdateAttributesForProfileWithName(
      kDeletedAccount.name,
      base::BindOnce(&UpdateAttributesFromTestAccount, kDeletedAccount));

  ProfileAttributesIOS attr =
      storage.GetAttributesForProfileWithName(kDeletedAccount.name);
  ASSERT_FALSE(attr.IsDeletedProfile());
  ASSERT_EQ(attr.GetGaiaId(), GaiaId(std::string(kDeletedAccount.gaia)));
  ASSERT_FALSE(attr.GetAttachedGaiaIds().empty());

  storage.MarkProfileForDeletion(kDeletedAccount.name);

  ASSERT_FALSE(storage.HasProfileWithName(kDeletedAccount.name));
  ASSERT_TRUE(storage.IsProfileMarkedForDeletion(kDeletedAccount.name));

  attr = storage.GetAttributesForProfileWithName(kDeletedAccount.name);
  EXPECT_EQ(attr.GetProfileName(), kDeletedAccount.name);
  EXPECT_TRUE(attr.IsDeletedProfile());
  EXPECT_EQ(attr.GetGaiaId(), GaiaId());
  EXPECT_TRUE(attr.GetAttachedGaiaIds().empty());
}

// Tests that UpdateAttributesForProfileWithName(...) does nothing if the
// profile is marked for deletion.
TEST_F(MutableProfileAttributesStorageIOSTest,
       UpdateAttributesForProfileWithName_DeletedProfile) {
  MutableProfileAttributesStorageIOS storage(pref_service());
  storage.AddProfile(kDeletedAccount.name);
  storage.MarkProfileForDeletion(kDeletedAccount.name);

  ASSERT_FALSE(storage.HasProfileWithName(kDeletedAccount.name));
  ASSERT_TRUE(storage.IsProfileMarkedForDeletion(kDeletedAccount.name));

  storage.UpdateAttributesForProfileWithName(
      kDeletedAccount.name,
      base::BindOnce(&UpdateAttributesFromTestAccount, kDeletedAccount));

  ProfileAttributesIOS attr =
      storage.GetAttributesForProfileWithName(kDeletedAccount.name);
  EXPECT_EQ(attr.GetProfileName(), kDeletedAccount.name);
  EXPECT_TRUE(attr.IsDeletedProfile());
  EXPECT_EQ(attr.GetGaiaId(), GaiaId());
  EXPECT_TRUE(attr.GetAttachedGaiaIds().empty());
}

// Tests that calling IterateOverProfileAttributes(...) with an iterator
// callback returning IterationResult can stops the iteration early and
// that mutations are reflected in the storage.
TEST_F(MutableProfileAttributesStorageIOSTest,
       IterateOverProfileAttributes_Iterator) {
  MutableProfileAttributesStorageIOS storage(pref_service());
  for (const TestAccount& account : kTestAccounts) {
    storage.AddProfile(account.name);
    storage.UpdateAttributesForProfileWithName(
        account.name,
        base::BindOnce(&UpdateAttributesFromTestAccount, account));
  }

  // Add a profile and mark it as deleted, to check that the iteration
  // does not iterate over deleted profiles.
  storage.AddProfile(kDeletedAccount.name);
  storage.UpdateAttributesForProfileWithName(
      kDeletedAccount.name,
      base::BindOnce(&UpdateAttributesFromTestAccount, kDeletedAccount));
  storage.MarkProfileForDeletion(kDeletedAccount.name);

  ASSERT_TRUE(storage.IsProfileMarkedForDeletion(kDeletedAccount.name));
  ASSERT_EQ(storage.GetNumberOfProfiles(), std::size(kTestAccounts));
  ASSERT_GT(storage.GetNumberOfProfiles(), 1u);

  std::string modified;
  const base::Time timestamp = base::Time::Now();
  storage.IterateOverProfileAttributes(base::BindRepeating(
      [](std::string& name, base::Time timestamp, ProfileAttributesIOS& attr) {
        EXPECT_NE(attr.GetProfileName(), kDeletedAccount.name);
        name = attr.GetProfileName();
        attr.SetLastActiveTime(timestamp);
        return ProfileAttributesStorageIOS::IterationResult::kTerminate;
      },
      std::ref(modified), timestamp));

  // The iterator should have been called at least once.
  ASSERT_FALSE(modified.empty());
  ASSERT_NE(modified, kDeletedAccount.name);

  for (const TestAccount& account : kTestAccounts) {
    const auto attr = storage.GetAttributesForProfileWithName(account.name);
    if (attr.GetProfileName() == modified) {
      EXPECT_EQ(attr.GetLastActiveTime(), timestamp);
    } else {
      EXPECT_NE(attr.GetLastActiveTime(), timestamp);
    }
  }
}

// Tests that calling IterateOverProfileAttributes(...) with an iterator
// callback returning void will iterate over all items and that mutations
// are reflected in the storage.
TEST_F(MutableProfileAttributesStorageIOSTest,
       IterateOverProfileAttributes_CompleteIterator) {
  MutableProfileAttributesStorageIOS storage(pref_service());
  for (const TestAccount& account : kTestAccounts) {
    storage.AddProfile(account.name);
    storage.UpdateAttributesForProfileWithName(
        account.name,
        base::BindOnce(&UpdateAttributesFromTestAccount, account));
  }

  // Add a profile and mark it as deleted, to check that the iteration
  // does not iterate over deleted profiles.
  storage.AddProfile(kDeletedAccount.name);
  storage.UpdateAttributesForProfileWithName(
      kDeletedAccount.name,
      base::BindOnce(&UpdateAttributesFromTestAccount, kDeletedAccount));
  storage.MarkProfileForDeletion(kDeletedAccount.name);

  ASSERT_TRUE(storage.IsProfileMarkedForDeletion(kDeletedAccount.name));
  ASSERT_EQ(storage.GetNumberOfProfiles(), std::size(kTestAccounts));
  ASSERT_GT(storage.GetNumberOfProfiles(), 1u);

  const base::Time timestamp = base::Time::Now();
  storage.IterateOverProfileAttributes(base::BindRepeating(
      [](base::Time timestamp, ProfileAttributesIOS& attr) {
        EXPECT_NE(attr.GetProfileName(), kDeletedAccount.name);
        attr.SetLastActiveTime(timestamp);
      },
      timestamp));

  for (const TestAccount& account : kTestAccounts) {
    const auto attr = storage.GetAttributesForProfileWithName(account.name);
    EXPECT_EQ(attr.GetLastActiveTime(), timestamp);
  }
}

// Tests that calling IterateOverProfileAttributes(...) with an iterator
// callback returning IterationResult can stops the iteration early.
TEST_F(MutableProfileAttributesStorageIOSTest,
       IterateOverProfileAttributes_ConstIterator) {
  MutableProfileAttributesStorageIOS storage(pref_service());
  for (const TestAccount& account : kTestAccounts) {
    storage.AddProfile(account.name);
    storage.UpdateAttributesForProfileWithName(
        account.name,
        base::BindOnce(&UpdateAttributesFromTestAccount, account));
  }

  // Add a profile and mark it as deleted, to check that the iteration
  // does not iterate over deleted profiles.
  storage.AddProfile(kDeletedAccount.name);
  storage.UpdateAttributesForProfileWithName(
      kDeletedAccount.name,
      base::BindOnce(&UpdateAttributesFromTestAccount, kDeletedAccount));
  storage.MarkProfileForDeletion(kDeletedAccount.name);

  ASSERT_TRUE(storage.IsProfileMarkedForDeletion(kDeletedAccount.name));
  ASSERT_EQ(storage.GetNumberOfProfiles(), std::size(kTestAccounts));
  ASSERT_GT(storage.GetNumberOfProfiles(), 1u);

  size_t counter = 0;
  storage.IterateOverProfileAttributes(base::BindRepeating(
      [](size_t& counter, const ProfileAttributesIOS& attr) {
        EXPECT_NE(attr.GetProfileName(), kDeletedAccount.name);
        ++counter;
        return ProfileAttributesStorageIOS::IterationResult::kTerminate;
      },
      std::ref(counter)));
  EXPECT_EQ(counter, 1u);
}

// Tests that calling IterateOverProfileAttributes(...) with an iterator
// callback returning void will iterate over all items.
TEST_F(MutableProfileAttributesStorageIOSTest,
       IterateOverProfileAttributes_ConstCompleteIterator) {
  MutableProfileAttributesStorageIOS storage(pref_service());
  for (const TestAccount& account : kTestAccounts) {
    storage.AddProfile(account.name);
    storage.UpdateAttributesForProfileWithName(
        account.name,
        base::BindOnce(&UpdateAttributesFromTestAccount, account));
  }

  // Add a profile and mark it as deleted, to check that the iteration
  // does not iterate over deleted profiles.
  storage.AddProfile(kDeletedAccount.name);
  storage.UpdateAttributesForProfileWithName(
      kDeletedAccount.name,
      base::BindOnce(&UpdateAttributesFromTestAccount, kDeletedAccount));
  storage.MarkProfileForDeletion(kDeletedAccount.name);

  ASSERT_TRUE(storage.IsProfileMarkedForDeletion(kDeletedAccount.name));
  ASSERT_EQ(storage.GetNumberOfProfiles(), std::size(kTestAccounts));
  ASSERT_GT(storage.GetNumberOfProfiles(), 1u);

  // Test that calling IterateOverProfileAttributes(...) with an iterator
  // callback returning void will iterate over all items.
  size_t counter = 0;
  storage.IterateOverProfileAttributes(base::BindRepeating(
      [](size_t& counter, const ProfileAttributesIOS& attr) {
        EXPECT_NE(attr.GetProfileName(), kDeletedAccount.name);
        ++counter;
      },
      std::ref(counter)));
  EXPECT_EQ(counter, storage.GetNumberOfProfiles());
}

TEST_F(MutableProfileAttributesStorageIOSTest, FixInvalidPersonalProfileName) {
  {
    // Setup: Register some profiles.
    MutableProfileAttributesStorageIOS storage(pref_service());
    for (const TestAccount& account : kTestAccounts) {
      storage.AddProfile(account.name);
    }
    ASSERT_EQ(storage.GetNumberOfProfiles(), std::size(kTestAccounts));
    // Set a personal profile name.
    storage.SetPersonalProfileName(kTestAccounts[0].name);
  }

  // For some reason, the prefs get corrupted, and the personal profile name
  // now points to a profile that doesn't actually exist. (Either the personal
  // profile name pref itself got corrupted somehow, or the profile entry got
  // removed.)
  const std::string kOtherProfileName = "other";
  pref_service()->SetString(prefs::kPersonalProfileName, kOtherProfileName);

  {
    MutableProfileAttributesStorageIOS storage(pref_service());

    EXPECT_EQ(storage.GetPersonalProfileName(), kOtherProfileName);
    EXPECT_TRUE(storage.HasProfileWithName(kOtherProfileName));
    EXPECT_EQ(storage.GetNumberOfProfiles(), std::size(kTestAccounts) + 1);
  }
}

TEST_F(MutableProfileAttributesStorageIOSTest,
       AllowEmptyInvalidPersonalProfileName) {
  {
    // Setup: Register some profiles.
    MutableProfileAttributesStorageIOS storage(pref_service());
    for (const TestAccount& account : kTestAccounts) {
      storage.AddProfile(account.name);
    }
    ASSERT_EQ(storage.GetNumberOfProfiles(), std::size(kTestAccounts));
    // The personal profile name wasn't set yet.
    ASSERT_TRUE(storage.GetPersonalProfileName().empty());
  }

  {
    MutableProfileAttributesStorageIOS storage(pref_service());
    ASSERT_EQ(storage.GetNumberOfProfiles(), std::size(kTestAccounts));

    // The personal profile name should still be empty.
    EXPECT_TRUE(storage.GetPersonalProfileName().empty());
  }
}

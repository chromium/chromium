// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_profile_mapper.h"

#import <optional>

#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_file_util.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "base/uuid.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/test/fake_change_profile_commands.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/mutable_profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_observer_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/scoped_profile_keep_alive_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using testing::_;
using testing::Ne;
using testing::UnorderedElementsAre;

namespace {

// Name of the personal profile.
constexpr char kPersonalProfileName[] = "4827c83a-573c-4701-94b3-622597db84fe";

FakeSystemIdentity* gmail_identity1 =
    [FakeSystemIdentity identityWithEmail:@"foo1@gmail.com"];
FakeSystemIdentity* gmail_identity2 =
    [FakeSystemIdentity identityWithEmail:@"foo2@gmail.com"];
FakeSystemIdentity* google_identity =
    [FakeSystemIdentity identityWithEmail:@"foo3@google.com"];
FakeSystemIdentity* chromium_identity =
    [FakeSystemIdentity identityWithEmail:@"foo4@chromium.com"];

// Helper for implementing FindCreatedProfileName(...). Stops iteration and
// store the profile name to `profile_name` when finding a profile that is
// not present in `known_profile_names`.
ProfileAttributesStorageIOS::IterationResult FindCreatedProfileNameHelper(
    std::string& profile_name,
    const base::flat_set<std::string>& known_profile_names,
    const ProfileAttributesIOS& attr) {
  if (!known_profile_names.contains(attr.GetProfileName())) {
    profile_name = attr.GetProfileName();
    return ProfileAttributesStorageIOS::IterationResult::kTerminate;
  }
  return ProfileAttributesStorageIOS::IterationResult::kContinue;
}

class MockObserver : public AccountProfileMapper::Observer {
 public:
  MockObserver() = default;
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  MOCK_METHOD(void, OnIdentitiesInProfileChanged, (), (override));
  MOCK_METHOD(void, OnIdentitiesOnDeviceChanged, (), (override));
  MOCK_METHOD(void,
              OnIdentityInProfileUpdated,
              (id<SystemIdentity>),
              (override));
  MOCK_METHOD(void,
              OnIdentityOnDeviceUpdated,
              (id<SystemIdentity>),
              (override));
  MOCK_METHOD(void,
              OnIdentityRefreshTokenUpdated,
              (id<SystemIdentity>),
              (override));
};

class MockAttributesStorageObserver
    : public ProfileAttributesStorageObserverIOS {
 public:
  MockAttributesStorageObserver() = default;
  MockAttributesStorageObserver(const MockAttributesStorageObserver&) = delete;
  MockAttributesStorageObserver& operator=(
      const MockAttributesStorageObserver&) = delete;
  ~MockAttributesStorageObserver() override = default;

  MOCK_METHOD(void, OnProfileAttributesUpdated, (std::string_view), (override));
};

// An "empty" implementation of ProfileIOS, used here to avoid using
// TestProfileIOS which pulls in a ton of dependencies (basically all
// KeyedServices).
class FakeProfileIOS : public ProfileIOS {
 public:
  explicit FakeProfileIOS(std::string_view profile_name)
      : ProfileIOS(base::CreateUniqueTempDirectoryScopedToTest(),
                   profile_name,
                   base::ThreadPool::CreateSequencedTaskRunner(
                       {base::MayBlock(),
                        base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

  bool IsOffTheRecord() const override { NOTREACHED(); }

  ProfileIOS* GetOriginalProfile() override { NOTREACHED(); }
  bool HasOffTheRecordProfile() const override { NOTREACHED(); }
  ProfileIOS* GetOffTheRecordProfile() override { NOTREACHED(); }
  void DestroyOffTheRecordProfile() override { NOTREACHED(); }
  ProfilePolicyConnector* GetPolicyConnector() override { NOTREACHED(); }
  policy::UserCloudPolicyManager* GetUserCloudPolicyManager() override {
    NOTREACHED();
  }
  sync_preferences::PrefServiceSyncable* GetSyncablePrefs() override {
    NOTREACHED();
  }
  const sync_preferences::PrefServiceSyncable* GetSyncablePrefs()
      const override {
    NOTREACHED();
  }
  ProfileIOSIOData* GetIOData() override { NOTREACHED(); }
  void ClearNetworkingHistorySince(base::Time time,
                                   base::OnceClosure completion) override {
    NOTREACHED();
  }
  PrefProxyConfigTracker* GetProxyConfigTracker() override { NOTREACHED(); }
  net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers) override {
    NOTREACHED();
  }
  base::WeakPtr<ProfileIOS> AsWeakPtr() override { NOTREACHED(); }
};

// A very minimal implementation of ProfileManagerIOS, implementing only the
// APIs used by AccountProfileMapper. (Note that the general
// TestProfileManagerIOS doesn't support profile creation, so isn't usable in
// these tests.)
class FakeProfileManagerIOS : public ProfileManagerIOS {
 public:
  explicit FakeProfileManagerIOS(PrefService* local_state)
      : profile_attributes_storage_(local_state) {
    // Create and load a profile, and mark it as the personal profile. This is
    // similar to what the real ProfileManagerIOS does on startup.
    profiles_map_[std::string(kPersonalProfileName)] =
        std::make_unique<FakeProfileIOS>(kPersonalProfileName);
    profile_attributes_storage_.AddProfile(kPersonalProfileName);
    profile_attributes_storage_.SetPersonalProfileName(kPersonalProfileName);
  }
  ~FakeProfileManagerIOS() override = default;

  void PrepareForDestruction() override { NOTREACHED(); }

  void AddObserver(ProfileManagerObserverIOS* observer) override {
    NOTREACHED();
  }
  void RemoveObserver(ProfileManagerObserverIOS* observer) override {
    NOTREACHED();
  }

  ProfileIOS* GetProfileWithName(std::string_view name) override {
    if (IsProfileMarkedForDeletion(name)) {
      return nullptr;
    }
    auto it = profiles_map_.find(name);
    if (it != profiles_map_.end()) {
      return it->second.get();
    }
    return nullptr;
  }

  std::vector<ProfileIOS*> GetLoadedProfiles() const override {
    std::vector<ProfileIOS*> profiles;
    for (const auto& [name, profile] : profiles_map_) {
      profiles.push_back(profile.get());
    }
    return profiles;
  }

  bool HasProfileWithName(std::string_view name) const override {
    return profile_attributes_storage_.HasProfileWithName(name);
  }

  bool CanCreateProfileWithName(std::string_view name) const override {
    return profile_attributes_storage_.CanCreateProfileWithName(name);
  }

  std::string ReserveNewProfileName() override {
    std::string profile_name;
    do {
      const base::Uuid uuid = base::Uuid::GenerateRandomV4();
      profile_name = uuid.AsLowercaseString();
    } while (!CanCreateProfileWithName(profile_name));

    profile_attributes_storage_.AddProfile(profile_name);
    return profile_name;
  }

  bool CanDeleteProfileWithName(std::string_view name) const override {
    return HasProfileWithName(name) &&
           name != profile_attributes_storage_.GetPersonalProfileName();
  }

  bool LoadProfileAsync(std::string_view name,
                        ProfileLoadedCallback initialized_callback,
                        ProfileLoadedCallback created_callback) override {
    NOTREACHED();
  }

  bool CreateProfileAsync(std::string_view name,
                          ProfileLoadedCallback initialized_callback,
                          ProfileLoadedCallback created_callback) override {
    profiles_map_[std::string(name)] = std::make_unique<FakeProfileIOS>(name);

    profile_attributes_storage_.AddProfile(name);

    ProfileIOS* profile = profiles_map_.find(name)->second.get();
    if (created_callback) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(created_callback),
                                    CreateScopedProfileKeepAlive(profile)));
    }
    if (initialized_callback) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(initialized_callback),
                                    CreateScopedProfileKeepAlive(profile)));
    }
    return true;
  }

  void MarkProfileForDeletion(std::string_view name) override {
    DCHECK(CanDeleteProfileWithName(name));
    profile_attributes_storage_.MarkProfileForDeletion(name);
  }

  bool IsProfileMarkedForDeletion(std::string_view name) const override {
    return profile_attributes_storage_.IsProfileMarkedForDeletion(name);
  }

  void PurgeProfilesMarkedForDeletion(base::OnceClosure callback) override {
    NOTREACHED();
  }

  ProfileAttributesStorageIOS* GetProfileAttributesStorage() override {
    return &profile_attributes_storage_;
  }

  base::FilePath GetProfilePath(std::string_view name) override {
    NOTREACHED();
  }

 private:
  ScopedProfileKeepAliveIOS CreateScopedProfileKeepAlive(ProfileIOS* profile) {
    return ScopedProfileKeepAliveIOS(CreatePassKey(), profile, {});
  }

  MutableProfileAttributesStorageIOS profile_attributes_storage_;

  std::map<std::string, std::unique_ptr<FakeProfileIOS>, std::less<>>
      profiles_map_;
};

class AccountProfileMapperTest : public PlatformTest {
 public:
  AccountProfileMapperTest() {
    profile_manager_ = std::make_unique<FakeProfileManagerIOS>(
        GetApplicationContext()->GetLocalState());

    system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
  }

  ~AccountProfileMapperTest() override = default;

  [[nodiscard]] NSArray* GetIdentitiesForProfile(
      std::string_view profile_name) {
    CHECK(profile_manager_->HasProfileWithName(profile_name));

    NSMutableArray* identities = [NSMutableArray array];
    auto callback = base::BindRepeating(
        [](NSMutableArray* identities, id<SystemIdentity> identity) {
          EXPECT_FALSE([identities containsObject:identity]);
          [identities addObject:identity];
          return AccountProfileMapper::IteratorResult::kContinueIteration;
        },
        identities);
    account_profile_mapper_->IterateOverIdentities(callback, profile_name);
    return identities;
  }

  std::string FindCreatedProfileName(
      const base::flat_set<std::string>& known_profile_names) {
    const ProfileAttributesStorageIOS* storage = profile_attributes_storage();
    EXPECT_EQ(storage->GetNumberOfProfiles(), known_profile_names.size() + 1);

    std::string profile_name;
    storage->IterateOverProfileAttributes(
        base::BindRepeating(&FindCreatedProfileNameHelper,
                            std::ref(profile_name), known_profile_names));

    CHECK(!profile_name.empty());
    return profile_name;
  }

  ProfileAttributesStorageIOS* profile_attributes_storage() {
    return profile_manager_->GetProfileAttributesStorage();
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<FakeProfileManagerIOS> profile_manager_;
  raw_ptr<FakeSystemIdentityManager> system_identity_manager_;
  std::unique_ptr<AccountProfileMapper> account_profile_mapper_;
};

TEST_F(AccountProfileMapperTest, NoIdentity) {
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());
  testing::StrictMock<MockObserver> mock_observer;
  account_profile_mapper_->AddObserver(&mock_observer, kPersonalProfileName);

  EXPECT_NSEQ(@[], GetIdentitiesForProfile(kPersonalProfileName));

  account_profile_mapper_->RemoveObserver(&mock_observer, kPersonalProfileName);
}

// Tests that `OnIdentitiesInProfileChanged()` and
// `OnIdentitiesOnDeviceChanged()` are called on the appropriate profile(s) when
// identities are added/removed.
TEST_F(AccountProfileMapperTest, IdentityListNotification) {
  const std::string kTestProfile1Name("11111111-1111-1111-1111-111111111111");
  const std::string kTestProfile2Name("ffffffff-ffff-ffff-ffff-ffffffffffff");

  base::test::TestFuture<ScopedProfileKeepAliveIOS> profile_initialized;
  profile_manager_->CreateProfileAsync(
      kTestProfile1Name, profile_initialized.GetCallback(), base::DoNothing());
  ASSERT_TRUE(profile_initialized.Wait());
  profile_manager_->CreateProfileAsync(
      kTestProfile2Name, profile_initialized.GetCallback(), base::DoNothing());
  ASSERT_TRUE(profile_initialized.Wait());

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  testing::StrictMock<MockObserver> mock_personal_observer;
  account_profile_mapper_->AddObserver(&mock_personal_observer,
                                       kPersonalProfileName);

  // The matching observer (for the personal profile) should get notified.
  EXPECT_CALL(mock_personal_observer, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_personal_observer, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(gmail_identity1);

  // *Only* the matching observer should get notified about
  // identities-in-profile changes.
  testing::StrictMock<MockObserver> mock_test1_observer;
  account_profile_mapper_->AddObserver(&mock_test1_observer, kTestProfile1Name);
  testing::StrictMock<MockObserver> mock_test2_observer;
  account_profile_mapper_->AddObserver(&mock_test2_observer, kTestProfile2Name);

  EXPECT_CALL(mock_personal_observer, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_test1_observer, OnIdentitiesInProfileChanged()).Times(0);
  EXPECT_CALL(mock_test2_observer, OnIdentitiesInProfileChanged()).Times(0);
  // But all observers should get notified about identities-on-device changes.
  EXPECT_CALL(mock_personal_observer, OnIdentitiesOnDeviceChanged());
  EXPECT_CALL(mock_test1_observer, OnIdentitiesOnDeviceChanged());
  EXPECT_CALL(mock_test2_observer, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(gmail_identity2);

  profile_initialized.Clear();
  base::RunLoop().RunUntilIdle();
}

// Tests that `OnIdentityRefreshTokenUpdated()` is called when the refresh
// token is updated. This should be done to the observer of the identity.
TEST_F(AccountProfileMapperTest, RefreshTokenNotification) {
  const std::string kTestProfile1Name("TestProfile1");
  base::test::TestFuture<ScopedProfileKeepAliveIOS> profile_initialized;
  profile_manager_->CreateProfileAsync(
      kTestProfile1Name, profile_initialized.GetCallback(), base::DoNothing());
  ASSERT_TRUE(profile_initialized.Wait());

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());
  testing::StrictMock<MockObserver> mock_personal_observer;
  account_profile_mapper_->AddObserver(&mock_personal_observer,
                                       kPersonalProfileName);
  testing::StrictMock<MockObserver> mock_profile1_observer;
  account_profile_mapper_->AddObserver(&mock_profile1_observer,
                                       kTestProfile1Name);

  EXPECT_CALL(mock_personal_observer, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_personal_observer, OnIdentitiesOnDeviceChanged());
  EXPECT_CALL(mock_profile1_observer, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_personal_observer,
              OnIdentityRefreshTokenUpdated(gmail_identity1));
  system_identity_manager_->FireIdentityRefreshTokenUpdatedNotification(
      gmail_identity1);

  account_profile_mapper_->RemoveObserver(&mock_personal_observer,
                                          kPersonalProfileName);
  account_profile_mapper_->RemoveObserver(&mock_profile1_observer,
                                          kTestProfile1Name);
}

// Tests that 2 non-managed identities are added to the personal profile.
TEST_F(AccountProfileMapperTest,
       NonManagedIdentitiesAreAssignedToPersonalProfile) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());
  testing::StrictMock<MockObserver> mock_observer;
  account_profile_mapper_->AddObserver(&mock_observer, kPersonalProfileName);

  EXPECT_CALL(mock_observer, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_observer, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_observer, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(gmail_identity2);

  NSArray* expected_identities = @[ gmail_identity1, gmail_identity2 ];
  EXPECT_NSEQ(expected_identities,
              GetIdentitiesForProfile(kPersonalProfileName));
  // Check that no other profiles have been created.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  account_profile_mapper_->RemoveObserver(&mock_observer, kPersonalProfileName);
}

// Tests that the 2 non-managed identities are added to the personal profile,
// and the managed identity is added to a newly-created separate profile.
TEST_F(AccountProfileMapperTest, ManagedIdentityIsAssignedToSeparateProfile) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());
  testing::StrictMock<MockObserver> mock_observer_personal;
  account_profile_mapper_->AddObserver(&mock_observer_personal,
                                       kPersonalProfileName);

  EXPECT_CALL(mock_observer_personal, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_observer_personal, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer_personal, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_observer_personal, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(gmail_identity2);

  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
  EXPECT_CALL(mock_observer_personal, OnIdentitiesInProfileChanged()).Times(0);
  EXPECT_CALL(mock_observer_personal, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(google_identity);
  // A new enterprise profile should've been registered.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);

  // Find the name of the new profile.
  std::string managed_profile_name = FindCreatedProfileName(
      /*known_profile_names=*/{std::string(kPersonalProfileName)});
  ASSERT_FALSE(managed_profile_name.empty());

  testing::StrictMock<MockObserver> mock_observer_managed;
  account_profile_mapper_->AddObserver(&mock_observer_managed,
                                       managed_profile_name);

  // Ensure identity-in-profile events get forwarded (only) to the appropriate
  // observer, while identity-on-device events get forwarded to all observers.
  EXPECT_CALL(mock_observer_personal,
              OnIdentityInProfileUpdated(gmail_identity2));
  EXPECT_CALL(mock_observer_personal,
              OnIdentityOnDeviceUpdated(gmail_identity2));
  EXPECT_CALL(mock_observer_managed,
              OnIdentityOnDeviceUpdated(gmail_identity2));
  system_identity_manager_->FireIdentityUpdatedNotification(gmail_identity2);

  EXPECT_CALL(mock_observer_managed,
              OnIdentityInProfileUpdated(google_identity));
  EXPECT_CALL(mock_observer_managed,
              OnIdentityOnDeviceUpdated(google_identity));
  EXPECT_CALL(mock_observer_personal,
              OnIdentityOnDeviceUpdated(google_identity));
  system_identity_manager_->FireIdentityUpdatedNotification(google_identity);

  // Verify the assignment of identities to profiles.
  NSArray* expected_identities_personal = @[ gmail_identity1, gmail_identity2 ];
  EXPECT_NSEQ(expected_identities_personal,
              GetIdentitiesForProfile(kPersonalProfileName));
  NSArray* expected_identities_managed = @[ google_identity ];
  EXPECT_NSEQ(expected_identities_managed,
              GetIdentitiesForProfile(managed_profile_name));

  account_profile_mapper_->RemoveObserver(&mock_observer_personal,
                                          kPersonalProfileName);
  account_profile_mapper_->RemoveObserver(&mock_observer_managed,
                                          managed_profile_name);
}

// Tests that the first managed identity is added to a newly-created profile,
// and the second managed identity is added to a *different* newly-created
// profile.
TEST_F(AccountProfileMapperTest,
       TwoManagedIdentitiesAreAssignedToTwoSeparateProfiles) {
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());
  testing::StrictMock<MockObserver> mock_observer_personal;
  account_profile_mapper_->AddObserver(&mock_observer_personal,
                                       kPersonalProfileName);

  EXPECT_CALL(mock_observer_personal, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_observer_personal, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer_personal, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_observer_personal, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(gmail_identity2);

  // Add a managed identity. This should trigger the registration of a new
  // profile.
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
  EXPECT_CALL(mock_observer_personal, OnIdentitiesInProfileChanged()).Times(0);
  EXPECT_CALL(mock_observer_personal, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(google_identity);
  // A new enterprise profile should've been registered.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);

  // Find the name of the new profile.
  std::string managed_profile_name1 = FindCreatedProfileName(
      /*known_profile_names=*/{std::string(kPersonalProfileName)});
  ASSERT_FALSE(managed_profile_name1.empty());

  testing::StrictMock<MockObserver> mock_observer_managed1;
  account_profile_mapper_->AddObserver(&mock_observer_managed1,
                                       managed_profile_name1);

  // Add another managed identity. This should again trigger the registration of
  // a new profile.
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);
  EXPECT_CALL(mock_observer_personal, OnIdentitiesInProfileChanged()).Times(0);
  EXPECT_CALL(mock_observer_personal, OnIdentitiesOnDeviceChanged());
  EXPECT_CALL(mock_observer_managed1, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(chromium_identity);
  // A new enterprise profile should've been registered.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 3u);

  // Find the name of the new profile.
  std::string managed_profile_name2 = FindCreatedProfileName(
      /*known_profile_names=*/{std::string(kPersonalProfileName),
                               managed_profile_name1});
  ASSERT_FALSE(managed_profile_name2.empty());

  testing::StrictMock<MockObserver> mock_observer_managed2;
  account_profile_mapper_->AddObserver(&mock_observer_managed2,
                                       managed_profile_name2);

  // Verify the assignments of identities to profiles.
  NSArray* expected_identities_personal = @[ gmail_identity1, gmail_identity2 ];
  EXPECT_NSEQ(expected_identities_personal,
              GetIdentitiesForProfile(kPersonalProfileName));
  NSArray* expected_identities_managed1 = @[ google_identity ];
  EXPECT_NSEQ(expected_identities_managed1,
              GetIdentitiesForProfile(managed_profile_name1));
  NSArray* expected_identities_managed2 = @[ chromium_identity ];
  EXPECT_NSEQ(expected_identities_managed2,
              GetIdentitiesForProfile(managed_profile_name2));

  account_profile_mapper_->RemoveObserver(&mock_observer_personal,
                                          kPersonalProfileName);
  account_profile_mapper_->RemoveObserver(&mock_observer_managed1,
                                          managed_profile_name1);
  account_profile_mapper_->RemoveObserver(&mock_observer_managed2,
                                          managed_profile_name2);
}

// Tests that a non-managed identity is removed correctly from the personal
// profile, and a managed identity is removed correctly from its profile.
TEST_F(AccountProfileMapperTest, IdentitiesAreRemovedFromCorrectProfile) {
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());
  testing::StrictMock<MockObserver> mock_observer_personal;
  account_profile_mapper_->AddObserver(&mock_observer_personal,
                                       kPersonalProfileName);
  EXPECT_CALL(mock_observer_personal, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_observer_personal, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer_personal, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_observer_personal, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(gmail_identity2);

  // Add a managed identity. This should trigger the creation of a new profile.
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
  EXPECT_CALL(mock_observer_personal, OnIdentitiesInProfileChanged()).Times(0);
  EXPECT_CALL(mock_observer_personal, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(google_identity);
  // A new enterprise profile should've been registered.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);

  // Find the name of the new profile.
  std::string managed_profile_name = FindCreatedProfileName(
      /*known_profile_names=*/{std::string(kPersonalProfileName)});
  ASSERT_FALSE(managed_profile_name.empty());

  testing::StrictMock<MockObserver> mock_observer_managed;
  account_profile_mapper_->AddObserver(&mock_observer_managed,
                                       managed_profile_name);

  // Remove a personal identity.
  {
    EXPECT_CALL(mock_observer_personal, OnIdentitiesInProfileChanged());
    EXPECT_CALL(mock_observer_personal, OnIdentitiesOnDeviceChanged());
    EXPECT_CALL(mock_observer_managed, OnIdentitiesOnDeviceChanged());
    base::RunLoop run_loop;
    system_identity_manager_->ForgetIdentity(
        gmail_identity2, base::BindOnce(
                             [](base::RunLoop* run_loop, NSError* error) {
                               EXPECT_EQ(nil, error);
                               run_loop->Quit();
                             },
                             &run_loop));
    run_loop.Run();
  }

  // The identity should've been removed from the personal profile.
  ASSERT_TRUE(profile_manager_->HasProfileWithName(kPersonalProfileName));
  NSArray* expected_identities_personal = @[ gmail_identity1 ];
  EXPECT_NSEQ(expected_identities_personal,
              GetIdentitiesForProfile(kPersonalProfileName));
  // The managed profile should be unaffected.
  ASSERT_TRUE(profile_manager_->HasProfileWithName(managed_profile_name));
  NSArray* expected_identities_managed = @[ google_identity ];
  EXPECT_NSEQ(expected_identities_managed,
              GetIdentitiesForProfile(managed_profile_name));

  // Remove the managed identity.
  {
    EXPECT_CALL(mock_observer_managed, OnIdentitiesInProfileChanged());
    EXPECT_CALL(mock_observer_managed, OnIdentitiesOnDeviceChanged());
    EXPECT_CALL(mock_observer_personal, OnIdentitiesOnDeviceChanged());
    base::RunLoop run_loop;
    system_identity_manager_->ForgetIdentity(
        google_identity, base::BindOnce(
                             [](base::RunLoop* run_loop, NSError* error) {
                               EXPECT_EQ(nil, error);
                               run_loop->Quit();
                             },
                             &run_loop));
    run_loop.Run();
  }

  // The personal profile should not have been affected.
  ASSERT_TRUE(profile_manager_->HasProfileWithName(kPersonalProfileName));
  expected_identities_personal = @[ gmail_identity1 ];
  EXPECT_NSEQ(expected_identities_personal,
              GetIdentitiesForProfile(kPersonalProfileName));
  // The managed profile should have been deleted.
  EXPECT_FALSE(profile_manager_->HasProfileWithName(managed_profile_name));

  account_profile_mapper_->RemoveObserver(&mock_observer_personal,
                                          kPersonalProfileName);
  account_profile_mapper_->RemoveObserver(&mock_observer_managed,
                                          managed_profile_name);
}

// Tests that only a single profile is created for a managed identity.
TEST_F(AccountProfileMapperTest, OnlyOneProfilePerIdentity) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  system_identity_manager_->AddIdentity(gmail_identity1);

  // Add a managed identity, which kicks off the async creation of a profile.
  system_identity_manager_->AddIdentity(google_identity);
  // Before the profile creation completes, do something else which triggers
  // OnIdentitiesInProfileChanged() and thus (re-)assignment of identities to
  // profiles. This should *not* kick off creation of another profile.
  system_identity_manager_->AddIdentity(gmail_identity2);

  // Exactly one new enterprise profile should've been registered.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);
}

// Tests that the mapping between profiles and accounts is populated when the
// AccountProfileMapper is created, even without any notifications from
// SystemIdentityManager.
TEST_F(AccountProfileMapperTest, IdentitiesAreAssignedOnStartup) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  // Some identities already exist before the AccountProfileMapper is created.
  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(google_identity);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  // A new enterprise profile should've been registered.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);

  // Find the name of the new profile.
  std::string managed_profile_name = FindCreatedProfileName(
      /*known_profile_names=*/{std::string(kPersonalProfileName)});
  ASSERT_FALSE(managed_profile_name.empty());

  // Verify the assignment of identities to profiles.
  NSArray* expected_identities_personal = @[ gmail_identity1 ];
  EXPECT_NSEQ(expected_identities_personal,
              GetIdentitiesForProfile(kPersonalProfileName));
  NSArray* expected_identities_managed = @[ google_identity ];
  EXPECT_NSEQ(expected_identities_managed,
              GetIdentitiesForProfile(managed_profile_name));
}

// Tests that if a managed account was the primary account pre-multi-profile, it
// remains the primary account in the personal profile (and does *not* get moved
// to its own managed profile).
TEST_F(AccountProfileMapperTest,
       AssignsPrimaryManagedAccountToPersonalProfile) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  // A managed identity exists on the device, and is set as the primary account
  // in the personal profile. It is *not* assigned to the profile though (as in
  // GetAttachedGaiaIds()), since the signin predates this mapping.
  system_identity_manager_->AddIdentity(google_identity);
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAuthenticationInfo(
            google_identity.gaiaId,
            base::SysNSStringToUTF8(google_identity.userFullName));
      }));

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  // The identity should have been attached to the personal profile (even though
  // it's a managed identity), and no additional profile should've been
  // registered.
  EXPECT_THAT(profile_attributes_storage()
                  ->GetAttributesForProfileWithName(kPersonalProfileName)
                  .GetAttachedGaiaIds(),
              UnorderedElementsAre(google_identity.gaiaId));
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
}

// Tests that if a managed account was the primary account pre-multi-profile,
// the personal profile gets migrated to become a managed profile, and a new
// personal profile is created for the rest of the personal accounts.
TEST_F(AccountProfileMapperTest, AssignsPrimaryManagedAccountToManagedProfile) {
  // A managed identity exists on the device, and is set as the primary account
  // in the personal profile. It is *not* assigned to the profile though (as in
  // GetAttachedGaiaIds()), since the signin predates this mapping.
  system_identity_manager_->AddIdentity(google_identity);
  system_identity_manager_->AddIdentity(gmail_identity1);
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAuthenticationInfo(
            google_identity.gaiaId,
            base::SysNSStringToUTF8(google_identity.userFullName));
        attr.SetAttachedGaiaIds(
            {gmail_identity1.gaiaId, google_identity.gaiaId});
      }));
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  // The managed identity should be attached to its managed profile, which is
  // the old personal profile that got converted to managed. And a new personal
  // profile gets created for the personal identity.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);
  const std::string new_personal_profile_name =
      profile_attributes_storage()->GetPersonalProfileName();
  const std::string new_managed_profile_name = kPersonalProfileName;
  EXPECT_NE(new_personal_profile_name, new_managed_profile_name);
  EXPECT_THAT(profile_attributes_storage()
                  ->GetAttributesForProfileWithName(new_managed_profile_name)
                  .GetAttachedGaiaIds(),
              UnorderedElementsAre(google_identity.gaiaId));
  EXPECT_THAT(profile_attributes_storage()
                  ->GetAttributesForProfileWithName(new_personal_profile_name)
                  .GetAttachedGaiaIds(),
              UnorderedElementsAre(gmail_identity1.gaiaId));
  EXPECT_NSEQ(@[ google_identity ],
              GetIdentitiesForProfile(new_managed_profile_name));
  EXPECT_NSEQ(@[ gmail_identity1 ],
              GetIdentitiesForProfile(new_personal_profile_name));
}

// Tests that if a managed account is assigned to the personal profile, but is
// not the primary account of that profile, it gets reassigned into its own
// dedicated profile.
TEST_F(AccountProfileMapperTest, ReassignsNonPrimaryIdentity) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  // A consumer identity and a managed identity already exist before the
  // AccountProfileMapper is created.
  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(google_identity);

  // Both identities are already assigned to the personal profile, but neither
  // of them is the primary one. This can happen in the following scenario:
  // * Pre-multi-profile, the managed account is the primary one.
  // * Multi-profile gets enabled.
  // * The managed account is signed out.
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce([](ProfileAttributesIOS& attr) {
        // Note: No `attr.SetAuthenticationInfo(...)` call, so no primary
        // account.
        attr.SetAttachedGaiaIds(
            {gmail_identity1.gaiaId, google_identity.gaiaId});
      }));
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  // The managed identity should have been reassigned to a new dedicated
  // profile.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);

  const std::string managed_profile_name = FindCreatedProfileName(
      /*known_profile_names=*/{kPersonalProfileName});

  // Each identity should be attached to the appropriate profile.
  EXPECT_THAT(profile_attributes_storage()
                  ->GetAttributesForProfileWithName(kPersonalProfileName)
                  .GetAttachedGaiaIds(),
              UnorderedElementsAre(gmail_identity1.gaiaId));
  EXPECT_THAT(profile_attributes_storage()
                  ->GetAttributesForProfileWithName(managed_profile_name)
                  .GetAttachedGaiaIds(),
              UnorderedElementsAre(google_identity.gaiaId));
}

// Tests that the personal profile gets correctly converted into a managed
// profile on MakePersonalProfileManagedWithGaiaID(), and a new personal profile
// gets created.
TEST_F(AccountProfileMapperTest, ConvertsPersonalProfileToManaged) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  // A personal and a managed account get added.
  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(google_identity);

  // Two profiles should be registered, the personal one and a managed one, each
  // with the appropriate account assigned to it.
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);

  const std::string original_personal_profile_name =
      profile_attributes_storage()->GetPersonalProfileName();
  ASSERT_TRUE(
      profile_manager_->HasProfileWithName(original_personal_profile_name));
  NSArray* expected_identities_personal = @[ gmail_identity1 ];
  ASSERT_NSEQ(expected_identities_personal,
              GetIdentitiesForProfile(original_personal_profile_name));

  const std::string original_managed_profile_name = FindCreatedProfileName(
      /*known_profile_names=*/{original_personal_profile_name});
  ASSERT_FALSE(original_managed_profile_name.empty());
  ASSERT_TRUE(
      profile_manager_->HasProfileWithName(original_managed_profile_name));
  NSArray* expected_identities_managed = @[ google_identity ];
  ASSERT_NSEQ(expected_identities_managed,
              GetIdentitiesForProfile(original_managed_profile_name));

  // The observer for the original personal profile (which will become a
  // managed profile) should get notified - regression test for
  // crbug.com/389733584.
  testing::StrictMock<MockObserver> mock_observer;
  account_profile_mapper_->AddObserver(&mock_observer,
                                       original_personal_profile_name);
  EXPECT_CALL(mock_observer, OnIdentitiesInProfileChanged());

  // Simulate that the user signs in with the managed account, and chooses to
  // take existing local data along, i.e. convert the personal profile into a
  // managed profile.
  account_profile_mapper_->MakePersonalProfileManagedWithGaiaID(
      google_identity.gaiaId);

  // What should have happened:
  // * The original personal profile should have become managed.
  // * The original managed profile should be gone.
  // * A new personal profile should have been registered.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);
  EXPECT_TRUE(
      profile_manager_->HasProfileWithName(original_personal_profile_name));
  EXPECT_FALSE(
      profile_manager_->HasProfileWithName(original_managed_profile_name));
  const std::string new_personal_profile_name =
      profile_attributes_storage()->GetPersonalProfileName();
  const std::string new_managed_profile_name = FindCreatedProfileName(
      /*known_profile_names=*/{new_personal_profile_name});
  EXPECT_NE(new_personal_profile_name, original_personal_profile_name);
  EXPECT_EQ(new_managed_profile_name, original_personal_profile_name);

  // The accounts should be assigned to the appropriate *new* profiles.
  EXPECT_NSEQ(expected_identities_personal,
              GetIdentitiesForProfile(new_personal_profile_name));
  EXPECT_NSEQ(expected_identities_managed,
              GetIdentitiesForProfile(new_managed_profile_name));
}

// Tests that the personal profile gets correctly converted into a managed
// profile on MakePersonalProfileManagedWithGaiaID(), and a new personal profile
// gets created.
//
// This test is identical to *.ConvertsPersonalProfileToManaged but set a
// ChangeProfileCommands handler. Together the two tests checks that both
// code path (with and without a ChangeProfileCommands) work.
TEST_F(AccountProfileMapperTest,
       ConvertsPersonalProfileToManaged_UsingChangeProfileCommands) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  FakeChangeProfileCommands* handler = [[FakeChangeProfileCommands alloc]
      initWithProfileManager:profile_manager_.get()];

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());
  account_profile_mapper_->SetChangeProfileCommandsHandler(handler);

  // A personal and a managed account get added.
  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(google_identity);

  // Two profiles should be registered, the personal one and a managed one, each
  // with the appropriate account assigned to it.
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);

  const std::string original_personal_profile_name =
      profile_attributes_storage()->GetPersonalProfileName();
  ASSERT_TRUE(
      profile_manager_->HasProfileWithName(original_personal_profile_name));
  NSArray* expected_identities_personal = @[ gmail_identity1 ];
  ASSERT_NSEQ(expected_identities_personal,
              GetIdentitiesForProfile(original_personal_profile_name));

  const std::string original_managed_profile_name = FindCreatedProfileName(
      /*known_profile_names=*/{original_personal_profile_name});
  ASSERT_FALSE(original_managed_profile_name.empty());
  ASSERT_TRUE(
      profile_manager_->HasProfileWithName(original_managed_profile_name));
  NSArray* expected_identities_managed = @[ google_identity ];
  ASSERT_NSEQ(expected_identities_managed,
              GetIdentitiesForProfile(original_managed_profile_name));

  // The observer for the original personal profile (which will become a
  // managed profile) should get notified - regression test for
  // crbug.com/389733584.
  testing::StrictMock<MockObserver> mock_observer;
  account_profile_mapper_->AddObserver(&mock_observer,
                                       original_personal_profile_name);
  EXPECT_CALL(mock_observer, OnIdentitiesInProfileChanged());
  EXPECT_FALSE(profile_manager_->IsProfileMarkedForDeletion(
      original_managed_profile_name));

  // Simulate that the user signs in with the managed account, and chooses to
  // take existing local data along, i.e. convert the personal profile into a
  // managed profile.
  account_profile_mapper_->MakePersonalProfileManagedWithGaiaID(
      google_identity.gaiaId);

  // What should have happened:
  // * The original personal profile should have become managed.
  // * The original managed profile should be gone.
  // * A new personal profile should have been registered.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);
  EXPECT_TRUE(
      profile_manager_->HasProfileWithName(original_personal_profile_name));
  EXPECT_FALSE(
      profile_manager_->HasProfileWithName(original_managed_profile_name));
  const std::string new_personal_profile_name =
      profile_attributes_storage()->GetPersonalProfileName();
  const std::string new_managed_profile_name = FindCreatedProfileName(
      /*known_profile_names=*/{new_personal_profile_name});
  EXPECT_NE(new_personal_profile_name, original_personal_profile_name);
  EXPECT_EQ(new_managed_profile_name, original_personal_profile_name);
  EXPECT_TRUE(profile_manager_->IsProfileMarkedForDeletion(
      original_managed_profile_name));

  // The accounts should be assigned to the appropriate *new* profiles.
  EXPECT_NSEQ(expected_identities_personal,
              GetIdentitiesForProfile(new_personal_profile_name));
  EXPECT_NSEQ(expected_identities_managed,
              GetIdentitiesForProfile(new_managed_profile_name));

  // Ensure the object no longer reference the ProfileManagerIOS.
  [handler shutdown];
}

// Tests that when the SystemIdentityManager finds the hosted domain of a
// managed account asynchronously (i.e. it wasn't available in the cache yet),
// the AccountProfileMapper correctly assigns the managed account to a
// managed profile once the hosted domain becomes available.
TEST_F(AccountProfileMapperTest, FetchesHostedDomainAsynchronously) {
  // Setup FakeSystemIdentityManager to *not* synchronously return hosted
  // domains.
  system_identity_manager_->OverrideGetHostedDomainImplementation(
      base::BindRepeating(
          [](id<SystemIdentity> identity,
             SystemIdentityManager::HostedDomainCallback callback) {
            std::move(callback).Run(@"google.com", nil);
          }));

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  testing::StrictMock<MockObserver> mock_personal_observer;
  account_profile_mapper_->AddObserver(&mock_personal_observer,
                                       kPersonalProfileName);

  testing::NiceMock<MockAttributesStorageObserver> mock_attributes_observer;
  profile_attributes_storage()->AddObserver(&mock_attributes_observer);

  // Add a managed account. This should trigger the async fetch of the hosted
  // domain. The account should show up among the identities-on-device, but
  // should not be assigned to any profile yet.
  EXPECT_CALL(mock_personal_observer, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(google_identity);
  // A new enterprise profile should *not* have been registered yet, since the
  // hosted domain isn't known synchronously.
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
  ASSERT_NSEQ(@[], GetIdentitiesForProfile(kPersonalProfileName));

  // Wait for the hosted domain fetch to finish. Once it does, the account
  // should get assigned to a newly-created enterprise profile.
  base::test::TestFuture<void> future;
  EXPECT_CALL(mock_attributes_observer,
              OnProfileAttributesUpdated(Ne(kPersonalProfileName)))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);

  EXPECT_NSEQ(@[], GetIdentitiesForProfile(kPersonalProfileName));
  std::string managed_profile_name = FindCreatedProfileName(
      /*known_profile_names=*/{std::string(kPersonalProfileName)});
  EXPECT_NSEQ(@[ google_identity ],
              GetIdentitiesForProfile(managed_profile_name));

  profile_attributes_storage()->RemoveObserver(&mock_attributes_observer);
  account_profile_mapper_->RemoveObserver(&mock_personal_observer,
                                          kPersonalProfileName);
}

// Tests that if a hosted domain fetch fails, AccountProfileMapper retries with
// exponential backoff.
TEST_F(AccountProfileMapperTest, RetriesHostedDomainFetchWithBackoff) {
  // Setup FakeSystemIdentityManager to *not* synchronously return hosted
  // domains, and to fail hosted domain fetches for now.
  NSError* error = [NSError errorWithDomain:@"Fetch failed"
                                       code:123
                                   userInfo:nil];
  size_t num_hosted_domain_errors_returned = 0;
  system_identity_manager_->OverrideGetHostedDomainImplementation(
      base::BindRepeating(
          [](NSError* error, size_t* num_errors_returned,
             id<SystemIdentity> identity,
             SystemIdentityManager::HostedDomainCallback callback) {
            ++(*num_errors_returned);
            std::move(callback).Run(nil, error);
          },
          error, &num_hosted_domain_errors_returned));

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  system_identity_manager_->AddIdentity(google_identity);
  // A new enterprise profile should *not* have been registered yet, since the
  // hosted domain isn't known synchronously.
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
  ASSERT_NSEQ(@[], GetIdentitiesForProfile(kPersonalProfileName));

  ASSERT_EQ(num_hosted_domain_errors_returned, 0u);
  // Wait for one GetHostedDomain() attempt to happen, but not long enough for
  // a retry (the initial delay is 1 second).
  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(num_hosted_domain_errors_returned, 1u);
  // Wait for one retry.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(num_hosted_domain_errors_returned, 2u);

  // Now let the next retry succeed.
  system_identity_manager_->OverrideGetHostedDomainImplementation({});
  task_environment_.FastForwardBy(base::Seconds(2));

  // A managed profile should have been created, and the account assigned to it.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);
  EXPECT_NSEQ(@[], GetIdentitiesForProfile(kPersonalProfileName));
  std::string managed_profile_name = FindCreatedProfileName(
      /*known_profile_names=*/{std::string(kPersonalProfileName)});
  EXPECT_NSEQ(@[ google_identity ],
              GetIdentitiesForProfile(managed_profile_name));
}

// Tests that if a hosted domain fetch fails repeatedly, AccountProfileMapper
// stops retrying after some number of attempts.
TEST_F(AccountProfileMapperTest, StopsRetryingHostedDomainFetches) {
  // Setup FakeSystemIdentityManager to *not* synchronously return hosted
  // domains, and to fail hosted domain fetches.
  NSError* error = [NSError errorWithDomain:@"Fetch failed"
                                       code:123
                                   userInfo:nil];
  size_t num_hosted_domain_errors_returned = 0;
  system_identity_manager_->OverrideGetHostedDomainImplementation(
      base::BindRepeating(
          [](NSError* error, size_t* num_errors_returned,
             id<SystemIdentity> identity,
             SystemIdentityManager::HostedDomainCallback callback) {
            ++(*num_errors_returned);
            std::move(callback).Run(nil, error);
          },
          error, &num_hosted_domain_errors_returned));

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  system_identity_manager_->AddIdentity(google_identity);
  // A new enterprise profile should *not* have been registered yet, since the
  // hosted domain isn't known synchronously.
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
  ASSERT_NSEQ(@[], GetIdentitiesForProfile(kPersonalProfileName));

  // Wait for a long time. This should *not* result in indefinite retrying;
  // rather, AccountProfileMapper should give up eventually, after a total of
  // five attempts.
  task_environment_.FastForwardBy(base::Hours(10));
  EXPECT_EQ(num_hosted_domain_errors_returned, 5u);
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
  EXPECT_NSEQ(@[], GetIdentitiesForProfile(kPersonalProfileName));

  system_identity_manager_->OverrideGetHostedDomainImplementation({});
}

// Tests that `Signin.IOSAccountProfileMappingStartupState` is not recorded when
// there are no managed accounts on the device (signed out or only personal
// accounts in the personal profile).
TEST_F(AccountProfileMapperTest, StartupStateHistogram_NoManagedAccounts) {
  constexpr char kStartupStateHistogram[] =
      "Signin.IOSAccountProfileMappingStartupState";
  constexpr char kStartupStateHasDefaultHistogram[] =
      "Signin.IOSAccountProfileMappingStartupState.HasDefaultProfile";
  constexpr char kStartupStateNoDefaultHistogram[] =
      "Signin.IOSAccountProfileMappingStartupState.NoDefaultProfile";

  // Signed out (no accounts).
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectTotalCount(kStartupStateHistogram, 0);
    histogram_tester.ExpectTotalCount(kStartupStateHasDefaultHistogram, 0);
    histogram_tester.ExpectTotalCount(kStartupStateNoDefaultHistogram, 0);
  }

  // Only personal accounts attached to `kPersonalProfileName`.
  account_profile_mapper_.reset();
  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(gmail_identity2);
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds(
            {gmail_identity1.gaiaId, gmail_identity2.gaiaId});
      }));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectTotalCount(kStartupStateHistogram, 0);
    histogram_tester.ExpectTotalCount(kStartupStateHasDefaultHistogram, 0);
    histogram_tester.ExpectTotalCount(kStartupStateNoDefaultHistogram, 0);
  }
}

// Tests that `Signin.IOSAccountProfileMappingStartupState` and its
// `.NoDefaultProfile` / `.HasDefaultProfile` variants record
// `kEachManagedAccountInSingleProfile` once per startup for healthy single or
// multiple managed accounts.
TEST_F(AccountProfileMapperTest, StartupStateHistogram_HealthyManagedAccounts) {
  constexpr char kManagedProfile1[] = "11111111-1111-1111-1111-111111111111";
  constexpr char kDefaultProfile[] = "Default";
  constexpr char kStartupStateHistogram[] =
      "Signin.IOSAccountProfileMappingStartupState";
  constexpr char kStartupStateHasDefaultHistogram[] =
      "Signin.IOSAccountProfileMappingStartupState.HasDefaultProfile";
  constexpr char kStartupStateNoDefaultHistogram[] =
      "Signin.IOSAccountProfileMappingStartupState.NoDefaultProfile";

  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(google_identity);
  profile_manager_->CreateProfileAsync(kManagedProfile1, {}, {});
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({gmail_identity1.gaiaId});
      }));
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kManagedProfile1, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({google_identity.gaiaId});
      }));

  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(kStartupStateHistogram,
                                        IOSAccountProfileMappingStartupState::
                                            kEachManagedAccountInSingleProfile,
                                        1);
    histogram_tester.ExpectUniqueSample(kStartupStateNoDefaultHistogram,
                                        IOSAccountProfileMappingStartupState::
                                            kEachManagedAccountInSingleProfile,
                                        1);
    histogram_tester.ExpectTotalCount(kStartupStateHasDefaultHistogram, 0);
  }

  // Add a second managed account in `"Default"` and verify `.HasDefaultProfile`
  // still records a single sample per startup.
  account_profile_mapper_.reset();
  system_identity_manager_->AddIdentity(chromium_identity);
  profile_manager_->CreateProfileAsync(kDefaultProfile, {}, {});
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kDefaultProfile, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({chromium_identity.gaiaId});
      }));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(kStartupStateHistogram,
                                        IOSAccountProfileMappingStartupState::
                                            kEachManagedAccountInSingleProfile,
                                        1);
    histogram_tester.ExpectUniqueSample(kStartupStateHasDefaultHistogram,
                                        IOSAccountProfileMappingStartupState::
                                            kEachManagedAccountInSingleProfile,
                                        1);
    histogram_tester.ExpectTotalCount(kStartupStateNoDefaultHistogram, 0);
  }
}

// Tests that `Signin.IOSAccountProfileMappingStartupState` records Cohort A1
// and A2 (`UUID_M < "Default"`, `UUID_M` uninitialized, `"Default"`
// initialized, `UUID_P` uninitialized vs. initialized).
TEST_F(AccountProfileMapperTest, StartupStateHistogram_CohortA1AndA2) {
  constexpr char kManagedProfile[] = "11111111-1111-1111-1111-111111111111";
  constexpr char kDefaultProfile[] = "Default";
  constexpr char kStartupStateHistogram[] =
      "Signin.IOSAccountProfileMappingStartupState";
  constexpr char kStartupStateHasDefaultHistogram[] =
      "Signin.IOSAccountProfileMappingStartupState.HasDefaultProfile";

  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(google_identity);
  profile_manager_->CreateProfileAsync(kManagedProfile, {}, {});
  profile_manager_->CreateProfileAsync(kDefaultProfile, {}, {});

  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({gmail_identity1.gaiaId});
      }));
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kManagedProfile, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({google_identity.gaiaId});
      }));
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kDefaultProfile, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({google_identity.gaiaId});
        attr.SetFullyInitialized();
      }));

  // A1: Personal profile is uninitialized.
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortA1_ShadowedByUninitManaged_PersonalUninit,
        1);
    histogram_tester.ExpectUniqueSample(
        kStartupStateHasDefaultHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortA1_ShadowedByUninitManaged_PersonalUninit,
        1);
  }

  // A2: Personal profile is initialized.
  account_profile_mapper_.reset();
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetFullyInitialized();
      }));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortA2_ShadowedByUninitManaged_PersonalInit,
        1);
    histogram_tester.ExpectUniqueSample(
        kStartupStateHasDefaultHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortA2_ShadowedByUninitManaged_PersonalInit,
        1);
  }
}

// Tests that `Signin.IOSAccountProfileMappingStartupState` records Cohort A3a,
// A3b, and `SecondMoreRecent` (`UUID_M < "Default"`, both initialized, varying
// `LastActiveTime`).
TEST_F(AccountProfileMapperTest, StartupStateHistogram_CohortA3) {
  constexpr char kManagedProfile[] = "11111111-1111-1111-1111-111111111111";
  constexpr char kDefaultProfile[] = "Default";
  constexpr char kStartupStateHistogram[] =
      "Signin.IOSAccountProfileMappingStartupState";
  const base::Time t1 = base::Time::FromSecondsSinceUnixEpoch(100);
  const base::Time t2 = base::Time::FromSecondsSinceUnixEpoch(200);
  const base::Time t3 = base::Time::FromSecondsSinceUnixEpoch(300);

  system_identity_manager_->AddIdentity(google_identity);
  profile_manager_->CreateProfileAsync(kManagedProfile, {}, {});
  profile_manager_->CreateProfileAsync(kDefaultProfile, {}, {});

  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kManagedProfile, base::BindOnce(
                           [](base::Time t, ProfileAttributesIOS& attr) {
                             attr.SetAttachedGaiaIds({google_identity.gaiaId});
                             attr.SetFullyInitialized();
                             attr.SetLastActiveTime(t);
                           },
                           t2));
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kDefaultProfile, base::BindOnce(
                           [](base::Time t, ProfileAttributesIOS& attr) {
                             attr.SetAttachedGaiaIds({google_identity.gaiaId});
                             attr.SetFullyInitialized();
                             attr.SetLastActiveTime(t);
                           },
                           t1));
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce(
                                [](base::Time t, ProfileAttributesIOS& attr) {
                                  attr.SetFullyInitialized();
                                  attr.SetLastActiveTime(t);
                                },
                                t1));

  // A3a: `UUID_M` (`t2`) is more recently active than both `"Default"` (`t1`)
  // and `UUID_P` (`t1`).
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortA3a_TwoInitManaged_FirstMoreRecent,
        1);
  }

  // A3b: `UUID_P` (`t3`) is more recently active than `UUID_M` (`t2`), which is
  // more recently active than `"Default"` (`t1`).
  account_profile_mapper_.reset();
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce(
                                [](base::Time t, ProfileAttributesIOS& attr) {
                                  attr.SetLastActiveTime(t);
                                },
                                t3));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortA3b_TwoInitManaged_PersonalMoreRecent,
        1);
  }

  // A3c: `"Default"` (`t3`) is more recently active than `UUID_M` (`t2`).
  account_profile_mapper_.reset();
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kDefaultProfile, base::BindOnce(
                           [](base::Time t, ProfileAttributesIOS& attr) {
                             attr.SetLastActiveTime(t);
                           },
                           t3));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortA3c_TwoInitManaged_SecondMoreRecent,
        1);
  }
}

// Tests that `Signin.IOSAccountProfileMappingStartupState` records Cohort B1
// and B2 (`"Default" < UUID_M`, `UUID_M` uninitialized).
TEST_F(AccountProfileMapperTest, StartupStateHistogram_CohortB1AndB2) {
  constexpr char kDefaultProfile[] = "Default";
  constexpr char kManagedProfile[] = "ffffffff-ffff-ffff-ffff-ffffffffffff";
  constexpr char kStartupStateHistogram[] =
      "Signin.IOSAccountProfileMappingStartupState";
  const base::Time t1 = base::Time::FromSecondsSinceUnixEpoch(100);
  const base::Time t2 = base::Time::FromSecondsSinceUnixEpoch(200);

  system_identity_manager_->AddIdentity(google_identity);
  profile_manager_->CreateProfileAsync(kDefaultProfile, {}, {});
  profile_manager_->CreateProfileAsync(kManagedProfile, {}, {});

  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kDefaultProfile, base::BindOnce(
                           [](base::Time t, ProfileAttributesIOS& attr) {
                             attr.SetAttachedGaiaIds({google_identity.gaiaId});
                             attr.SetFullyInitialized();
                             attr.SetLastActiveTime(t);
                           },
                           t1));
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kManagedProfile, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({google_identity.gaiaId});
      }));

  // B1: Personal profile is uninitialized.
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortB1_ExtraUninitManaged_OrigMoreRecent,
        1);
  }

  // B2: Personal profile is initialized and more recently active (`t2`) than
  // `"Default"` (`t1`).
  account_profile_mapper_.reset();
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce(
                                [](base::Time t, ProfileAttributesIOS& attr) {
                                  attr.SetFullyInitialized();
                                  attr.SetLastActiveTime(t);
                                },
                                t2));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortB2_ExtraUninitManaged_PersonalMoreRecent,
        1);
  }
}

// Tests that `Signin.IOSAccountProfileMappingStartupState` records Cohort C1,
// C2, and C3 (`Gaia_M` attached to both `"Default"` and `UUID_P`, varying
// `UUID_P` initialization, primary account, and `LastActiveTime`, even when a
// second healthy managed account is present).
TEST_F(AccountProfileMapperTest, StartupStateHistogram_CohortC) {
  constexpr char kDefaultProfile[] = "Default";
  constexpr char kSecondManagedProfile[] =
      "22222222-2222-2222-2222-222222222222";
  constexpr char kPersonalProfile[] = "ffffffff-ffff-ffff-ffff-ffffffffffff";
  constexpr char kStartupStateHistogram[] =
      "Signin.IOSAccountProfileMappingStartupState";
  const base::Time t1 = base::Time::FromSecondsSinceUnixEpoch(100);
  const base::Time t2 = base::Time::FromSecondsSinceUnixEpoch(200);
  const base::Time t3 = base::Time::FromSecondsSinceUnixEpoch(300);

  system_identity_manager_->AddIdentity(google_identity);
  system_identity_manager_->AddIdentity(chromium_identity);
  profile_manager_->CreateProfileAsync(kDefaultProfile, {}, {});
  profile_manager_->CreateProfileAsync(kSecondManagedProfile, {}, {});
  profile_manager_->CreateProfileAsync(kPersonalProfile, {}, {});
  profile_attributes_storage()->SetPersonalProfileName(kPersonalProfile);

  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kDefaultProfile, base::BindOnce(
                           [](base::Time t, ProfileAttributesIOS& attr) {
                             attr.SetAttachedGaiaIds({google_identity.gaiaId});
                             attr.SetFullyInitialized();
                             attr.SetLastActiveTime(t);
                           },
                           t1));
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kSecondManagedProfile, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({chromium_identity.gaiaId});
        attr.SetFullyInitialized();
      }));
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfile, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({google_identity.gaiaId});
      }));

  // C1: Personal profile is uninitialized.
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortC1_PersonalAndManaged_PersonalUninit,
        1);
  }

  // C2: Personal profile is initialized, and `google_identity` is NOT the
  // primary account of the personal profile.
  account_profile_mapper_.reset();
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfile, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetFullyInitialized();
      }));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortC2_PersonalAndManaged_NotPrimaryInPersonal,
        1);
  }

  // C3: Personal profile is initialized, `google_identity` IS the primary
  // account of the personal profile, and personal (`t2`) is more recent than
  // managed (`t1`).
  account_profile_mapper_.reset();
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfile,
      base::BindOnce(
          [](base::Time t, ProfileAttributesIOS& attr) {
            attr.SetAuthenticationInfo(
                google_identity.gaiaId,
                base::SysNSStringToUTF8(google_identity.userEmail));
            attr.SetLastActiveTime(t);
          },
          t2));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortC3_PersonalAndManaged_PrimaryInPersonal,
        1);
  }

  // C3 edge case (`ManagedMoreRecent`): Managed profile (`t3`) was active more
  // recently than personal (`t2`).
  account_profile_mapper_.reset();
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kDefaultProfile, base::BindOnce(
                           [](base::Time t, ProfileAttributesIOS& attr) {
                             attr.SetLastActiveTime(t);
                           },
                           t3));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortC3_PersonalAndManaged_PrimaryInPersonal_ManagedMoreRecent,
        1);
  }
}

// Tests that `Signin.IOSAccountProfileMappingStartupState` records unexpected
// corruption buckets (`kCohortB_FirstIsDefaultUninit_SecondInit`,
// `kCohortB_TwoInitManaged_FirstIsDefault`, `kTwoUninitManagedProfiles`,
// `kThreeOrMoreProfiles`, and `kMultipleCorruptedManagedAccounts`).
TEST_F(AccountProfileMapperTest, StartupStateHistogram_UnexpectedBuckets) {
  constexpr char kDefaultProfile[] = "Default";
  constexpr char kManagedProfile1[] = "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee";
  constexpr char kManagedProfile2[] = "ffffffff-ffff-ffff-ffff-ffffffffffff";
  constexpr char kStartupStateHistogram[] =
      "Signin.IOSAccountProfileMappingStartupState";

  system_identity_manager_->AddIdentity(google_identity);
  system_identity_manager_->AddIdentity(chromium_identity);
  profile_manager_->CreateProfileAsync(kDefaultProfile, {}, {});
  profile_manager_->CreateProfileAsync(kManagedProfile1, {}, {});
  profile_manager_->CreateProfileAsync(kManagedProfile2, {}, {});

  // 1. `"Default" < kManagedProfile1`: `"Default"` uninitialized,
  // `kManagedProfile1` initialized
  // (`kCohortB_FirstIsDefaultUninit_SecondInit`).
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kDefaultProfile, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({google_identity.gaiaId});
      }));
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kManagedProfile1, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({google_identity.gaiaId});
        attr.SetFullyInitialized();
      }));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortB_FirstIsDefaultUninit_SecondInit,
        1);
  }

  // 2. `"Default" < kManagedProfile1`: both `"Default"` and `kManagedProfile1`
  // initialized (`kCohortB_TwoInitManaged_FirstIsDefault`).
  account_profile_mapper_.reset();
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kDefaultProfile, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetFullyInitialized();
      }));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::
            kCohortB_TwoInitManaged_FirstIsDefault,
        1);
  }

  // 3. `google_identity` attached to 2 uninitialized Managed profiles.
  account_profile_mapper_.reset();
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kDefaultProfile, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({});
      }));
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kManagedProfile1, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({google_identity.gaiaId});
        // Reset initialized state by clearing and re-adding or using
        // `kManagedProfile1` and `kManagedProfile2`:
      }));
  // Recreate `kManagedProfile1` uninitialized by detaching from it and using a
  // fresh uninitialized profile `kManagedProfile3`.
  constexpr char kManagedProfile3[] = "dddddddd-dddd-dddd-dddd-dddddddddddd";
  profile_manager_->CreateProfileAsync(kManagedProfile3, {}, {});
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kManagedProfile1, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({});
      }));
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kManagedProfile3, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({google_identity.gaiaId});
      }));
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kManagedProfile2, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({google_identity.gaiaId});
      }));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::kTwoUninitManagedProfiles, 1);
  }

  // 4. `google_identity` attached to 3 Managed profiles.
  account_profile_mapper_.reset();
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kManagedProfile1, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds({google_identity.gaiaId});
      }));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::kThreeOrMoreProfiles, 1);
  }

  // 5. Both `google_identity` and `chromium_identity` attached to >= 2
  // profiles.
  account_profile_mapper_.reset();
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kManagedProfile1, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds(
            {google_identity.gaiaId, chromium_identity.gaiaId});
      }));
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kManagedProfile2, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAttachedGaiaIds(
            {google_identity.gaiaId, chromium_identity.gaiaId});
      }));
  {
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(
        kStartupStateHistogram,
        IOSAccountProfileMappingStartupState::kMultipleCorruptedManagedAccounts,
        1);
  }
}

class AccountProfileMapperCleanupTest : public AccountProfileMapperTest {
 protected:
  static constexpr char kDefaultProfile[] = "Default";
  static constexpr char kStartupStateHistogram[] =
      "Signin.IOSAccountProfileMappingStartupState";
  static constexpr char kCleanupActionHistogram[] =
      "Signin.IOSAccountProfileMappingCleanupAction";
  static constexpr base::Time kOlderActiveTime =
      base::Time::FromSecondsSinceUnixEpoch(100);
  static constexpr base::Time kNewerActiveTime =
      base::Time::FromSecondsSinceUnixEpoch(200);

  // Creates `name` (if not already present) and configures its on-disk
  // attributes while no `AccountProfileMapper` is running.
  void ConfigureProfile(std::string_view name,
                        ProfileAttributesIOS::GaiaIdSet attached_gaia_ids,
                        bool initialized = false,
                        base::Time last_active_time = base::Time(),
                        FakeSystemIdentity* primary_identity = nil) {
    account_profile_mapper_.reset();
    if (!profile_manager_->HasProfileWithName(name)) {
      profile_manager_->CreateProfileAsync(name, {}, {});
    }
    profile_attributes_storage()->UpdateAttributesForProfileWithName(
        name, base::BindOnce(
                  [](ProfileAttributesIOS::GaiaIdSet gaia_ids, bool initialized,
                     base::Time last_active_time,
                     FakeSystemIdentity* primary_identity,
                     ProfileAttributesIOS& attr) {
                    attr.SetAttachedGaiaIds(gaia_ids);
                    if (initialized) {
                      attr.SetFullyInitialized();
                    }
                    if (!last_active_time.is_null()) {
                      attr.SetLastActiveTime(last_active_time);
                    }
                    if (primary_identity) {
                      attr.SetAuthenticationInfo(
                          primary_identity.gaiaId,
                          base::SysNSStringToUTF8(primary_identity.userEmail));
                    }
                  },
                  std::move(attached_gaia_ids), initialized, last_active_time,
                  primary_identity));
  }

  // Recreates `account_profile_mapper_` (simulating browser startup) and
  // verifies the startup-state and cleanup-action histograms.
  void RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState expected_startup_state,
      std::optional<IOSAccountProfileMappingCleanupAction>
          expected_cleanup_action,
      const base::Location& location = FROM_HERE) {
    SCOPED_TRACE(location.ToString());
    account_profile_mapper_.reset();
    base::HistogramTester histogram_tester;
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        system_identity_manager_, profile_manager_.get(),
        GetApplicationContext()->GetLocalState());

    histogram_tester.ExpectUniqueSample(kStartupStateHistogram,
                                        expected_startup_state, 1);
    if (expected_cleanup_action.has_value()) {
      histogram_tester.ExpectUniqueSample(kCleanupActionHistogram,
                                          *expected_cleanup_action, 1);
    } else {
      histogram_tester.ExpectTotalCount(kCleanupActionHistogram, 0);
    }
  }

  // Simulates a subsequent browser startup and verifies the mapping is healthy.
  void VerifyHealthyNextStartup(const base::Location& location = FROM_HERE) {
    RecreateMapperAndExpectHistograms(IOSAccountProfileMappingStartupState::
                                          kEachManagedAccountInSingleProfile,
                                      std::nullopt, location);
  }

  // Removes `identity` from `system_identity_manager_` and waits for
  // completion.
  void ForgetIdentitySync(id<SystemIdentity> identity) {
    base::test::TestFuture<NSError*> forget_future;
    system_identity_manager_->ForgetIdentity(identity,
                                             forget_future.GetCallback());
    EXPECT_EQ(forget_future.Get(), nil);
  }

  // Returns whether `name` is recorded in `prefs::kAbandonedProfiles`.
  bool IsProfileAbandoned(std::string_view name) const {
    return GetApplicationContext()
               ->GetLocalState()
               ->GetDict(prefs::kAbandonedProfiles)
               .FindDict(name) != nullptr;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      kCleanupCorruptedAccountProfileMappings};
};

// Tests that when `kCleanupCorruptedAccountProfileMappings` is disabled,
// corrupted mappings are left untouched and no cleanup histogram is emitted.
TEST_F(AccountProfileMapperCleanupTest, FeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      kCleanupCorruptedAccountProfileMappings);

  constexpr char kManagedProfile[] = "11111111-1111-1111-1111-111111111111";
  system_identity_manager_->AddIdentity(google_identity);
  ConfigureProfile(kManagedProfile, {google_identity.gaiaId},
                   /*initialized=*/false);
  ConfigureProfile(kDefaultProfile, {google_identity.gaiaId},
                   /*initialized=*/true);

  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::
          kCohortA1_ShadowedByUninitManaged_PersonalUninit,
      /*expected_cleanup_action=*/std::nullopt);

  EXPECT_TRUE(profile_manager_->HasProfileWithName(kManagedProfile));
  EXPECT_FALSE(profile_manager_->IsProfileMarkedForDeletion(kManagedProfile));
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kDefaultProfile));
  EXPECT_EQ(
      account_profile_mapper_->FindProfileNameForGaiaID(google_identity.gaiaId),
      kManagedProfile);
}

// Tests that `MaybeCleanUpCorruptedProfileMappings` deletes the uninitialized
// shadowing Managed profile (`UUID_M < "Default"`) in Cohort A1/A2, remapping
// scenes and `kLastUsedProfile` to `"Default"`, and restores `"Default"` as the
// sole profile for the managed account.
TEST_F(AccountProfileMapperCleanupTest, CohortA1AndA2) {
  constexpr char kManagedProfile[] = "11111111-1111-1111-1111-111111111111";

  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(google_identity);
  ConfigureProfile(kManagedProfile, {google_identity.gaiaId},
                   /*initialized=*/false);
  ConfigureProfile(kDefaultProfile, {google_identity.gaiaId},
                   /*initialized=*/true);
  ConfigureProfile(kPersonalProfileName, {gmail_identity1.gaiaId},
                   /*initialized=*/false);
  profile_attributes_storage()->SetProfileNameForSceneID("scene1",
                                                         kManagedProfile);
  GetApplicationContext()->GetLocalState()->SetString(prefs::kLastUsedProfile,
                                                      kManagedProfile);

  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::
          kCohortA1_ShadowedByUninitManaged_PersonalUninit,
      IOSAccountProfileMappingCleanupAction::kDeletedFirstManagedProfileUninit);

  EXPECT_FALSE(profile_manager_->HasProfileWithName(kManagedProfile));
  EXPECT_TRUE(profile_manager_->IsProfileMarkedForDeletion(kManagedProfile));
  EXPECT_EQ(profile_attributes_storage()->GetProfileNameForSceneID("scene1"),
            kDefaultProfile);
  EXPECT_EQ(GetApplicationContext()->GetLocalState()->GetString(
                prefs::kLastUsedProfile),
            kDefaultProfile);
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kDefaultProfile));
  EXPECT_EQ(
      account_profile_mapper_->FindProfileNameForGaiaID(google_identity.gaiaId),
      kDefaultProfile);
  EXPECT_NSEQ(@[ google_identity ], GetIdentitiesForProfile(kDefaultProfile));
  EXPECT_NSEQ(@[ gmail_identity1 ],
              GetIdentitiesForProfile(kPersonalProfileName));

  VerifyHealthyNextStartup();
}

// Tests that `MaybeCleanUpCorruptedProfileMappings` abandons the second
// Managed profile (`"Default"`) in Cohort A3 when the first Managed profile
// (`UUID_M`) has a more recent `LastActiveTime`, archiving `"Default"` in
// `prefs::kAbandonedProfiles` and remapping scenes / `kLastUsedProfile`, and
// marks `"Default"` for deletion when the managed account is later removed from
// the device.
TEST_F(AccountProfileMapperCleanupTest, CohortA3_KeepsFirstWhenMoreRecent) {
  constexpr char kManagedProfile[] = "11111111-1111-1111-1111-111111111111";

  system_identity_manager_->AddIdentity(google_identity);
  ConfigureProfile(kManagedProfile, {google_identity.gaiaId},
                   /*initialized=*/true, kNewerActiveTime);
  ConfigureProfile(kDefaultProfile, {google_identity.gaiaId},
                   /*initialized=*/true, kOlderActiveTime);
  profile_attributes_storage()->SetProfileNameForSceneID("scene1",
                                                         kDefaultProfile);
  profile_attributes_storage()->SetProfileNameForSceneID("scene2",
                                                         kPersonalProfileName);
  GetApplicationContext()->GetLocalState()->SetString(prefs::kLastUsedProfile,
                                                      kDefaultProfile);

  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::
          kCohortA3a_TwoInitManaged_FirstMoreRecent,
      IOSAccountProfileMappingCleanupAction::
          kAbandonedSecondManagedProfileInitOlder);

  EXPECT_FALSE(profile_manager_->HasProfileWithName(kDefaultProfile));
  EXPECT_FALSE(profile_manager_->IsProfileMarkedForDeletion(kDefaultProfile));
  EXPECT_FALSE(profile_manager_->CanCreateProfileWithName(kDefaultProfile));
  EXPECT_TRUE(IsProfileAbandoned(kDefaultProfile));
  EXPECT_EQ(profile_attributes_storage()->GetProfileNameForSceneID("scene1"),
            kManagedProfile);
  EXPECT_EQ(profile_attributes_storage()->GetProfileNameForSceneID("scene2"),
            kPersonalProfileName);
  EXPECT_EQ(GetApplicationContext()->GetLocalState()->GetString(
                prefs::kLastUsedProfile),
            kManagedProfile);
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kManagedProfile));
  EXPECT_EQ(
      account_profile_mapper_->FindProfileNameForGaiaID(google_identity.gaiaId),
      kManagedProfile);

  VerifyHealthyNextStartup();

  // Removing `google_identity` from the device at runtime should mark both the
  // kept profile and the abandoned profile for deletion.
  ForgetIdentitySync(google_identity);
  EXPECT_TRUE(profile_manager_->IsProfileMarkedForDeletion(kManagedProfile));
  EXPECT_TRUE(profile_manager_->IsProfileMarkedForDeletion(kDefaultProfile));
  EXPECT_FALSE(IsProfileAbandoned(kDefaultProfile));
}

// Tests that `MaybeCleanUpCorruptedProfileMappings` abandons the first
// Managed profile (`UUID_M`) in Cohort A3 when the second Managed profile
// (`"Default"`) has a more recent `LastActiveTime`.
TEST_F(AccountProfileMapperCleanupTest, CohortA3_KeepsSecondWhenMoreRecent) {
  constexpr char kManagedProfile[] = "11111111-1111-1111-1111-111111111111";

  system_identity_manager_->AddIdentity(google_identity);
  ConfigureProfile(kManagedProfile, {google_identity.gaiaId},
                   /*initialized=*/true, kOlderActiveTime);
  ConfigureProfile(kDefaultProfile, {google_identity.gaiaId},
                   /*initialized=*/true, kNewerActiveTime);

  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::
          kCohortA3c_TwoInitManaged_SecondMoreRecent,
      IOSAccountProfileMappingCleanupAction::
          kAbandonedFirstManagedProfileInitOlder);

  EXPECT_FALSE(profile_manager_->HasProfileWithName(kManagedProfile));
  EXPECT_FALSE(profile_manager_->IsProfileMarkedForDeletion(kManagedProfile));
  EXPECT_FALSE(profile_manager_->CanCreateProfileWithName(kManagedProfile));
  EXPECT_TRUE(IsProfileAbandoned(kManagedProfile));
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kDefaultProfile));
  EXPECT_EQ(
      account_profile_mapper_->FindProfileNameForGaiaID(google_identity.gaiaId),
      kDefaultProfile);

  VerifyHealthyNextStartup();

  // Simulate `google_identity` being removed while Chrome is not running, even
  // if `kCleanupCorruptedAccountProfileMappings` has since been disabled: the
  // next startup should still mark both the kept profile and the abandoned
  // profile for deletion.
  account_profile_mapper_.reset();
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(
      kCleanupCorruptedAccountProfileMappings);
  ForgetIdentitySync(google_identity);
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());
  EXPECT_TRUE(profile_manager_->IsProfileMarkedForDeletion(kDefaultProfile));
  EXPECT_TRUE(profile_manager_->IsProfileMarkedForDeletion(kManagedProfile));
  EXPECT_FALSE(IsProfileAbandoned(kManagedProfile));
}

// Tests that `MaybeCleanUpCorruptedProfileMappings` deletes the uninitialized
// second Managed profile (`"Default" < UUID_M`) in Cohort B.
TEST_F(AccountProfileMapperCleanupTest, CohortB) {
  constexpr char kManagedProfile[] = "ffffffff-ffff-ffff-ffff-ffffffffffff";

  system_identity_manager_->AddIdentity(google_identity);
  ConfigureProfile(kDefaultProfile, {google_identity.gaiaId},
                   /*initialized=*/true);
  ConfigureProfile(kManagedProfile, {google_identity.gaiaId},
                   /*initialized=*/false);

  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::
          kCohortB1_ExtraUninitManaged_OrigMoreRecent,
      IOSAccountProfileMappingCleanupAction::
          kDeletedSecondManagedProfileUninit);

  EXPECT_FALSE(profile_manager_->HasProfileWithName(kManagedProfile));
  EXPECT_TRUE(profile_manager_->IsProfileMarkedForDeletion(kManagedProfile));
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kDefaultProfile));
  EXPECT_EQ(
      account_profile_mapper_->FindProfileNameForGaiaID(google_identity.gaiaId),
      kDefaultProfile);

  VerifyHealthyNextStartup();
}

// Tests that `MaybeCleanUpCorruptedProfileMappings` detaches the managed
// account from the Personal profile in Cohorts C1, C2, and C3, while leaving
// `AuthenticationInfo` intact in C3 until the Personal profile loads.
TEST_F(AccountProfileMapperCleanupTest, CohortC) {
  constexpr char kPersonalProfile[] = "ffffffff-ffff-ffff-ffff-ffffffffffff";

  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(google_identity);
  ConfigureProfile(kDefaultProfile, {google_identity.gaiaId},
                   /*initialized=*/true);
  ConfigureProfile(kPersonalProfile,
                   {gmail_identity1.gaiaId, google_identity.gaiaId},
                   /*initialized=*/false);
  profile_attributes_storage()->SetPersonalProfileName(kPersonalProfile);

  // 1. Cohort C1: Personal profile is uninitialized.
  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::
          kCohortC1_PersonalAndManaged_PersonalUninit,
      IOSAccountProfileMappingCleanupAction::
          kDetachedFromUninitPersonalProfile);
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kDefaultProfile));
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kPersonalProfile));
  EXPECT_NSEQ(@[ gmail_identity1 ], GetIdentitiesForProfile(kPersonalProfile));
  EXPECT_NSEQ(@[ google_identity ], GetIdentitiesForProfile(kDefaultProfile));

  // 2. Cohort C2: Personal profile is initialized, `google_identity` is NOT
  // primary in Personal.
  ConfigureProfile(kPersonalProfile,
                   {gmail_identity1.gaiaId, google_identity.gaiaId},
                   /*initialized=*/true, base::Time(), gmail_identity1);
  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::
          kCohortC2_PersonalAndManaged_NotPrimaryInPersonal,
      IOSAccountProfileMappingCleanupAction::
          kDetachedFromInitPersonalProfileNotPrimary);
  EXPECT_EQ(profile_attributes_storage()
                ->GetAttributesForProfileWithName(kPersonalProfile)
                .GetGaiaId(),
            gmail_identity1.gaiaId);
  EXPECT_NSEQ(@[ gmail_identity1 ], GetIdentitiesForProfile(kPersonalProfile));
  EXPECT_NSEQ(@[ google_identity ], GetIdentitiesForProfile(kDefaultProfile));

  // 3. Cohort C3: Personal profile is initialized, `google_identity` IS
  // primary in Personal. `AuthenticationInfo` remains intact until the Personal
  // profile loads so `PushNotificationAccountContextManager` refcounts stay
  // consistent.
  ConfigureProfile(kPersonalProfile,
                   {gmail_identity1.gaiaId, google_identity.gaiaId},
                   /*initialized=*/true, base::Time(), google_identity);
  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::
          kCohortC3_PersonalAndManaged_PrimaryInPersonal,
      IOSAccountProfileMappingCleanupAction::
          kDetachedFromInitPersonalProfilePrimary);
  EXPECT_EQ(profile_attributes_storage()
                ->GetAttributesForProfileWithName(kPersonalProfile)
                .GetGaiaId(),
            google_identity.gaiaId);
  EXPECT_NSEQ(@[ gmail_identity1 ], GetIdentitiesForProfile(kPersonalProfile));
  EXPECT_NSEQ(@[ google_identity ], GetIdentitiesForProfile(kDefaultProfile));
  EXPECT_EQ(
      account_profile_mapper_->FindProfileNameForGaiaID(google_identity.gaiaId),
      kDefaultProfile);

  VerifyHealthyNextStartup();
}

// Tests that `MaybeCleanUpCorruptedProfileMappings` skips identities that were
// removed from the device, consumer identities, and identities whose hosted
// domain is not yet cached (cleaning up the latter on a subsequent startup once
// the hosted domain is cached).
TEST_F(AccountProfileMapperCleanupTest,
       SkipsRemovedConsumerAndUncachedIdentities) {
  constexpr char kManagedProfile[] = "11111111-1111-1111-1111-111111111111";

  // 1. Removed identity (`google_identity` not on device): skipped by cleanup,
  // and both profiles are marked for deletion by
  // `UpdateIdentityProfileMappings()`.
  ConfigureProfile(kManagedProfile, {google_identity.gaiaId},
                   /*initialized=*/false);
  ConfigureProfile(kDefaultProfile, {google_identity.gaiaId},
                   /*initialized=*/true);
  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::
          kCohortA1_ShadowedByUninitManaged_PersonalUninit,
      /*expected_cleanup_action=*/std::nullopt);
  EXPECT_TRUE(profile_manager_->IsProfileMarkedForDeletion(kManagedProfile));
  EXPECT_TRUE(profile_manager_->IsProfileMarkedForDeletion(kDefaultProfile));

  // 2. Consumer identity (`gmail_identity1`) attached to both Personal and
  // Managed: skipped by cleanup so it is not detached from Personal;
  // `UpdateIdentityProfileMappings()` detaches it from the Managed profile.
  constexpr char kManagedProfile2[] = "22222222-2222-2222-2222-222222222222";
  account_profile_mapper_.reset();
  system_identity_manager_->AddIdentity(gmail_identity1);
  ConfigureProfile(kManagedProfile2, {gmail_identity1.gaiaId},
                   /*initialized=*/true);
  ConfigureProfile(kPersonalProfileName, {gmail_identity1.gaiaId},
                   /*initialized=*/true, base::Time(), gmail_identity1);
  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::
          kCohortC3_PersonalAndManaged_PrimaryInPersonal,
      /*expected_cleanup_action=*/std::nullopt);
  EXPECT_NSEQ(@[ gmail_identity1 ],
              GetIdentitiesForProfile(kPersonalProfileName));
  EXPECT_NSEQ(@[], GetIdentitiesForProfile(kManagedProfile2));

  // 3. Managed identity with uncached hosted domain (`nil` at startup): cleanup
  // is deferred on the first startup until the async hosted-domain fetch
  // populates the cache, and then runs on the next startup.
  constexpr char kManagedProfile3[] = "33333333-3333-3333-3333-333333333333";
  account_profile_mapper_.reset();
  system_identity_manager_->OverrideGetHostedDomainImplementation(
      base::BindRepeating(
          [](id<SystemIdentity> identity,
             SystemIdentityManager::HostedDomainCallback callback) {
            std::move(callback).Run(@"google.com", nil);
          }));
  system_identity_manager_->AddIdentity(google_identity);
  ASSERT_EQ(system_identity_manager_->GetCachedHostedDomainForIdentity(
                google_identity),
            nil);
  ConfigureProfile(kManagedProfile3, {google_identity.gaiaId},
                   /*initialized=*/false);
  ConfigureProfile(kManagedProfile2, {google_identity.gaiaId},
                   /*initialized=*/true);

  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::
          kCohortB1_ExtraUninitManaged_OrigMoreRecent,
      /*expected_cleanup_action=*/std::nullopt);
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kManagedProfile2));
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kManagedProfile3));

  // Let the async hosted-domain fetch populate the cache, then simulate the
  // next browser startup.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return system_identity_manager_->GetCachedHostedDomainForIdentity(
               google_identity) != nil;
  }));
  system_identity_manager_->WaitForServiceCallbacksToComplete();
  ASSERT_NSEQ(system_identity_manager_->GetCachedHostedDomainForIdentity(
                  google_identity),
              @"google.com");

  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::
          kCohortB1_ExtraUninitManaged_OrigMoreRecent,
      IOSAccountProfileMappingCleanupAction::
          kDeletedSecondManagedProfileUninit);
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kManagedProfile2));
  EXPECT_FALSE(profile_manager_->HasProfileWithName(kManagedProfile3));
  EXPECT_TRUE(profile_manager_->IsProfileMarkedForDeletion(kManagedProfile3));

  VerifyHealthyNextStartup();
}

// Tests that `MaybeCleanUpCorruptedProfileMappings` handles unexpected buckets
// (`kTwoUninitManagedProfiles`, `kThreeOrMoreProfiles`, and
// `kMultipleCorruptedManagedAccounts`).
TEST_F(AccountProfileMapperCleanupTest, UnexpectedBuckets) {
  constexpr char kManagedProfile1[] = "11111111-1111-1111-1111-111111111111";
  constexpr char kManagedProfile2[] = "22222222-2222-2222-2222-222222222222";
  constexpr char kManagedProfile3[] = "33333333-3333-3333-3333-333333333333";
  constexpr char kManagedProfile4[] = "44444444-4444-4444-4444-444444444444";
  constexpr char kManagedProfile5[] = "55555555-5555-5555-5555-555555555555";

  system_identity_manager_->AddIdentity(google_identity);

  // 1. Both Managed profiles uninitialized: deletes `kManagedProfile2` and
  // keeps `kManagedProfile1`.
  ConfigureProfile(kManagedProfile1, {google_identity.gaiaId});
  ConfigureProfile(kManagedProfile2, {google_identity.gaiaId});
  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::kTwoUninitManagedProfiles,
      IOSAccountProfileMappingCleanupAction::
          kDeletedSecondManagedProfileBothUninit);
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kManagedProfile1));
  EXPECT_FALSE(profile_manager_->HasProfileWithName(kManagedProfile2));
  EXPECT_TRUE(profile_manager_->IsProfileMarkedForDeletion(kManagedProfile2));

  // 2. Attached to 3 Managed profiles: skipped without mutation.
  ConfigureProfile(kManagedProfile3, {google_identity.gaiaId});
  ConfigureProfile(kManagedProfile4, {google_identity.gaiaId});
  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::kThreeOrMoreProfiles,
      IOSAccountProfileMappingCleanupAction::kSkippedThreeOrMoreProfiles);
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kManagedProfile1));
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kManagedProfile3));
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kManagedProfile4));

  // 3. Multiple corrupted accounts (`google_identity` and `chromium_identity`
  // attached to 2 Managed profiles each): skipped without mutation.
  system_identity_manager_->AddIdentity(chromium_identity);
  ConfigureProfile(kManagedProfile4, {chromium_identity.gaiaId});
  ConfigureProfile(kManagedProfile5, {chromium_identity.gaiaId});
  RecreateMapperAndExpectHistograms(
      IOSAccountProfileMappingStartupState::kMultipleCorruptedManagedAccounts,
      IOSAccountProfileMappingCleanupAction::kSkippedMultipleCorruptedAccounts);
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kManagedProfile1));
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kManagedProfile3));
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kManagedProfile4));
  EXPECT_TRUE(profile_manager_->HasProfileWithName(kManagedProfile5));
}

}  // namespace

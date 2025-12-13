// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_profile_mapper.h"

#import "base/containers/contains.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_file_util.h"
#import "base/test/test_future.h"
#import "base/uuid.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
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

@interface FakeChangeProfileCommands : NSObject <ChangeProfileCommands>

- (instancetype)initWithProfileManager:(ProfileManagerIOS*)manager
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)shutdown;

@property(nonatomic, readonly) BOOL deleteProfileCalled;

@end

@implementation FakeChangeProfileCommands {
  raw_ptr<ProfileManagerIOS, DanglingUntriaged> _manager;
}

- (instancetype)initWithProfileManager:(ProfileManagerIOS*)manager {
  if ((self = [super init])) {
    DCHECK(manager);
    _manager = manager;
  }
  return self;
}

- (void)shutdown {
  _manager = nullptr;
}

- (void)changeProfile:(std::string_view)profileName
             forScene:(SceneState*)sceneState
               reason:(ChangeProfileReason)reason
         continuation:(ChangeProfileContinuation)continuation {
  NOTREACHED();
}

- (void)deleteProfile:(std::string_view)profileName {
  _deleteProfileCalled = YES;
  _manager->MarkProfileForDeletion(profileName);
}

@end

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
    return !HasProfileWithName(name) && !IsProfileMarkedForDeletion(name);
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
  explicit AccountProfileMapperTest(
      bool separate_profiles_enabled,
      bool separate_profiles_force_migration_enabled) {
    features_.InitWithFeatureStates(
        {{kSeparateProfilesForManagedAccounts, separate_profiles_enabled},
         {kSeparateProfilesForManagedAccountsForceMigration,
          separate_profiles_force_migration_enabled}});

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

 private:
  base::test::ScopedFeatureList features_;
};

class AccountProfileMapperAccountsInSeparateProfilesTest
    : public AccountProfileMapperTest {
 public:
  AccountProfileMapperAccountsInSeparateProfilesTest()
      : AccountProfileMapperTest(
            /*separate_profiles_enabled=*/true,
            /*separate_profiles_force_migration_enabled=*/false) {}
  ~AccountProfileMapperAccountsInSeparateProfilesTest() override = default;
};

class AccountProfileMapperAccountsInSingleProfileTest
    : public AccountProfileMapperTest {
 public:
  AccountProfileMapperAccountsInSingleProfileTest()
      : AccountProfileMapperTest(
            /*separate_profiles_enabled=*/false,
            /*separate_profiles_force_migration_enabled=*/false) {}
  ~AccountProfileMapperAccountsInSingleProfileTest() override = default;
};

class AccountProfileMapperAccountsInSeparateProfilesWithForceMigrationTest
    : public AccountProfileMapperTest {
 public:
  AccountProfileMapperAccountsInSeparateProfilesWithForceMigrationTest()
      : AccountProfileMapperTest(
            /*separate_profiles_enabled=*/true,
            /*separate_profiles_force_migration_enabled=*/true) {}
  ~AccountProfileMapperAccountsInSeparateProfilesWithForceMigrationTest()
      override = default;

 private:
  base::test::ScopedFeatureList features_;
};

// Tests that AccountProfileMapper lists no identity when there are no
// identities.
TEST_F(AccountProfileMapperAccountsInSingleProfileTest, NoIdentity) {
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());
  testing::StrictMock<MockObserver> mock_observer;
  account_profile_mapper_->AddObserver(&mock_observer, kPersonalProfileName);

  EXPECT_NSEQ(@[], GetIdentitiesForProfile(kPersonalProfileName));

  account_profile_mapper_->RemoveObserver(&mock_observer, kPersonalProfileName);
}

TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       OnlyAvailableOnIos17Plus) {
    EXPECT_TRUE(AreSeparateProfilesForManagedAccountsEnabled());
}

TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest, NoIdentity) {
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
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       IdentityListNotification) {
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
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       RefreshTokenNotification) {
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

// Tests that `OnIdentityRefreshTokenUpdated()` is called when the refresh
// token is updated. This test is when having only one profile.
TEST_F(AccountProfileMapperAccountsInSingleProfileTest,
       RefreshTokenNotification) {
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());
  testing::StrictMock<MockObserver> mock_personal_observer;
  account_profile_mapper_->AddObserver(&mock_personal_observer,
                                       kPersonalProfileName);

  EXPECT_CALL(mock_personal_observer, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_personal_observer, OnIdentitiesOnDeviceChanged());
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_personal_observer,
              OnIdentityRefreshTokenUpdated(gmail_identity1));
  system_identity_manager_->FireIdentityRefreshTokenUpdatedNotification(
      gmail_identity1);

  account_profile_mapper_->RemoveObserver(&mock_personal_observer,
                                          kPersonalProfileName);
}

// Tests that when the feature flag is disabled, all identities are visible
// in all profiles.
TEST_F(AccountProfileMapperAccountsInSingleProfileTest,
       AllIdentitiesAreVisibleInAllProfiles) {
  const std::string kTestProfile1Name("TestProfile1");
  base::test::TestFuture<ScopedProfileKeepAliveIOS> profile_initialized;
  profile_manager_->CreateProfileAsync(
      kTestProfile1Name, profile_initialized.GetCallback(), base::DoNothing());
  ASSERT_TRUE(profile_initialized.Wait());

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());
  testing::StrictMock<MockObserver> mock_observer0;
  account_profile_mapper_->AddObserver(&mock_observer0, kPersonalProfileName);
  testing::StrictMock<MockObserver> mock_observer1;
  account_profile_mapper_->AddObserver(&mock_observer1, kTestProfile1Name);

  // Identity events should be forwarded to all observers.
  EXPECT_CALL(mock_observer0, OnIdentitiesInProfileChanged()).Times(3);
  EXPECT_CALL(mock_observer0, OnIdentitiesOnDeviceChanged()).Times(3);
  EXPECT_CALL(mock_observer1, OnIdentitiesInProfileChanged()).Times(3);
  EXPECT_CALL(mock_observer1, OnIdentitiesOnDeviceChanged()).Times(3);
  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(gmail_identity2);
  system_identity_manager_->AddIdentity(google_identity);

  EXPECT_CALL(mock_observer0, OnIdentityInProfileUpdated(gmail_identity1));
  EXPECT_CALL(mock_observer0, OnIdentityOnDeviceUpdated(gmail_identity1));
  EXPECT_CALL(mock_observer1, OnIdentityInProfileUpdated(gmail_identity1));
  EXPECT_CALL(mock_observer1, OnIdentityOnDeviceUpdated(gmail_identity1));
  system_identity_manager_->FireIdentityUpdatedNotification(gmail_identity1);

  // All identities should be visible in all profiles.
  NSArray* expected_identities =
      @[ gmail_identity1, gmail_identity2, google_identity ];
  EXPECT_NSEQ(expected_identities,
              GetIdentitiesForProfile(kPersonalProfileName));
  EXPECT_NSEQ(expected_identities, GetIdentitiesForProfile(kTestProfile1Name));

  // Remove an identity; this should also apply to all profiles.
  EXPECT_CALL(mock_observer0, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_observer0, OnIdentitiesOnDeviceChanged());
  EXPECT_CALL(mock_observer1, OnIdentitiesInProfileChanged());
  EXPECT_CALL(mock_observer1, OnIdentitiesOnDeviceChanged());
  base::RunLoop run_loop;
  system_identity_manager_->ForgetIdentity(
      gmail_identity2, base::BindOnce(
                           [](base::RunLoop* run_loop, NSError* error) {
                             EXPECT_EQ(nil, error);
                             run_loop->Quit();
                           },
                           &run_loop));
  run_loop.Run();

  // All (remaining) identities should be visible in all profiles.
  expected_identities = @[ gmail_identity1, google_identity ];
  EXPECT_NSEQ(expected_identities,
              GetIdentitiesForProfile(kPersonalProfileName));
  EXPECT_NSEQ(expected_identities, GetIdentitiesForProfile(kTestProfile1Name));

  account_profile_mapper_->RemoveObserver(&mock_observer1, kTestProfile1Name);
  account_profile_mapper_->RemoveObserver(&mock_observer0,
                                          kPersonalProfileName);
}

// Tests that 2 non-managed identities are added to the personal profile.
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
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
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       ManagedIdentityIsAssignedToSeparateProfile) {
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
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
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
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       IdentitiesAreRemovedFromCorrectProfile) {
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
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       OnlyOneProfilePerIdentity) {
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
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       IdentitiesAreAssignedOnStartup) {
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
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
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
// it stays in that state if the force-migration period is not reached yet.
TEST_F(AccountProfileMapperAccountsInSeparateProfilesWithForceMigrationTest,
       DoesNotAssignPrimaryManagedAccountToManagedProfile) {
  base::test::ScopedFeatureList feature_list;

  // A managed identity and a personal identity exist on the device. The managed
  // one is set as the primary account in the personal profile. It is *not*
  // assigned to the profile though (as in GetAttachedGaiaIds()), since the
  // signin predates this mapping.
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

  // Set the force migration time pref to still be less than the expected
  // duration.
  GetApplicationContext()->GetLocalState()->SetTime(
      prefs::kWaitingForMultiProfileForcedMigrationTimestamp,
      base::Time::Now() - base::Days(70));

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  // Both identities should stay attached to the personal profile.
  EXPECT_THAT(
      profile_attributes_storage()
          ->GetAttributesForProfileWithName(kPersonalProfileName)
          .GetAttachedGaiaIds(),
      UnorderedElementsAre(google_identity.gaiaId, gmail_identity1.gaiaId));
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
}

// Tests that if a managed account was the primary account pre-multi-profile,
// after force-migration period, the personal profile gets migrated to become a
// managed profile, and a new personal profile is created for the rest of the
// personal accounts.
TEST_F(AccountProfileMapperAccountsInSeparateProfilesWithForceMigrationTest,
       AssignsPrimaryManagedAccountToManagedProfile) {
  base::test::ScopedFeatureList feature_list;

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

  // Set the force migration time pref larger than the grace period.
  GetApplicationContext()->GetLocalState()->SetTime(
      prefs::kWaitingForMultiProfileForcedMigrationTimestamp,
      base::Time::Now() - base::Days(100));

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  // The managed identity should be attached to its managed profile, which is
  // the old personal profile that got converted to managed. And a new personal
  // profile gets created for the personal identity.
  EXPECT_THAT(profile_attributes_storage()
                  ->GetAttributesForProfileWithName(kPersonalProfileName)
                  .GetAttachedGaiaIds(),
              UnorderedElementsAre(google_identity.gaiaId));
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);
}

// Tests that a pre-existing identity which is the primary identity in a
// profile remains assigned to that profile, even if it'd now be assigned to a
// different one. This is important for managed accounts that pre-date the
// multi-profile support, since those shouldn't be automatically moved into a
// new profile.
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       DoesNotReassignPrimaryIdentity) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  // A consumer identity and a managed identity already exist before the
  // AccountProfileMapper is created.
  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(google_identity);

  // Both identities are already assigned to the personal profile, with the
  // managed identity being the primary one. This can happen if the identities
  // were added before multi-profile support was enabled.
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

  // Both identities should still be attached to the personal profile.
  EXPECT_THAT(
      profile_attributes_storage()
          ->GetAttributesForProfileWithName(kPersonalProfileName)
          .GetAttachedGaiaIds(),
      UnorderedElementsAre(gmail_identity1.gaiaId, google_identity.gaiaId));

  // No additional profile should've been registered.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
}

TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       ReassignsPrimaryIdentityOnSignout) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  // A consumer identity and a managed identity already exist before the
  // AccountProfileMapper is created.
  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(google_identity);

  // Both identities are already assigned to the personal profile, with the
  // managed identity being the primary one. This can happen if the identities
  // were added before multi-profile support was enabled.
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

  // Both identities are attached to the personal profile.
  ASSERT_THAT(
      profile_attributes_storage()
          ->GetAttributesForProfileWithName(kPersonalProfileName)
          .GetAttachedGaiaIds(),
      UnorderedElementsAre(gmail_identity1.gaiaId, google_identity.gaiaId));
  // Verify the force-migration pref is recorded.
  EXPECT_NE(GetApplicationContext()->GetLocalState()->GetTime(
                prefs::kWaitingForMultiProfileForcedMigrationTimestamp),
            base::Time());

  // No additional profile have been registered.
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  // Now the managed identity signs out, i.e. stops being the primary identity
  // in the personal profile.
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAuthenticationInfo(GaiaId(), "");
      }));

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

// Tests that if a managed account is assigned to the personal profile, but is
// not the primary account of that profile, it gets reassigned into its own
// dedicated profile.
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       ReassignsNonPrimaryIdentity) {
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
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       ConvertsPersonalProfileToManaged) {
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
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       ConvertsPersonalProfileToManaged_UsingChangeProfileCommands) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  FakeChangeProfileCommands* handler = [[FakeChangeProfileCommands alloc]
      initWithProfileManager:profile_manager_.get()];

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());
  account_profile_mapper_->SetChangeProfileCommandsHandler(handler);
  ASSERT_FALSE(handler.deleteProfileCalled);

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
  EXPECT_FALSE(handler.deleteProfileCalled);

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
  EXPECT_TRUE(handler.deleteProfileCalled);

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
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       FetchesHostedDomainAsynchronously) {
  // Setup FakeSystemIdentityManager to *not* synchronously return hosted
  // domains.
  system_identity_manager_->SetInstantlyFillHostedDomainCache(false);

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
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       RetriesHostedDomainFetchWithBackoff) {
  // Setup FakeSystemIdentityManager to *not* synchronously return hosted
  // domains, and to fail hosted domain fetches for now.
  system_identity_manager_->SetInstantlyFillHostedDomainCache(false);
  NSError* error = [NSError errorWithDomain:@"Fetch failed"
                                       code:123
                                   userInfo:nil];
  system_identity_manager_->SetGetHostedDomainError(error);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  system_identity_manager_->AddIdentity(google_identity);
  // A new enterprise profile should *not* have been registered yet, since the
  // hosted domain isn't known synchronously.
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
  ASSERT_NSEQ(@[], GetIdentitiesForProfile(kPersonalProfileName));

  ASSERT_EQ(system_identity_manager_->GetNumHostedDomainErrorsReturned(), 0u);
  // Wait for one GetHostedDomain() attempt to happen, but not long enough for
  // a retry (the initial delay is 1 second).
  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(system_identity_manager_->GetNumHostedDomainErrorsReturned(), 1u);
  // Wait for one retry.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(system_identity_manager_->GetNumHostedDomainErrorsReturned(), 2u);

  // Now let the next retry succeed.
  system_identity_manager_->SetGetHostedDomainError(nil);
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
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       StopsRetryingHostedDomainFetches) {
  // Setup FakeSystemIdentityManager to *not* synchronously return hosted
  // domains, and to fail hosted domain fetches.
  system_identity_manager_->SetInstantlyFillHostedDomainCache(false);
  NSError* error = [NSError errorWithDomain:@"Fetch failed"
                                       code:123
                                   userInfo:nil];
  system_identity_manager_->SetGetHostedDomainError(error);

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
  EXPECT_EQ(system_identity_manager_->GetNumHostedDomainErrorsReturned(), 5u);
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
  EXPECT_NSEQ(@[], GetIdentitiesForProfile(kPersonalProfileName));
}

// Tests that the force-migration pref is recorded for a managed account was the
// primary account pre-multi-profile, which remained the primary account in the
// personal profile (and did *not* get moved to its own managed profile).
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       ForceMigrationPrefRecordedForManagedAccountInPersonalProfile) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
  EXPECT_EQ(GetApplicationContext()->GetLocalState()->GetTime(
                prefs::kWaitingForMultiProfileForcedMigrationTimestamp),
            base::Time());

  // A managed identity exists on the device, and is set as the primary account
  // in the personal profile. It is *not* assigned to the profile though (as in
  // GetAttachedGaiaIds()), since the signin predates this mapping.
  system_identity_manager_->AddIdentity(google_identity);
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAuthenticationInfo(
            google_identity.gaiaId,
            base::SysNSStringToUTF8(google_identity.userFullName));
        attr.SetAttachedGaiaIds({google_identity.gaiaId});
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

  // Verify the force-migration pref is set.
  EXPECT_NE(GetApplicationContext()->GetLocalState()->GetTime(
                prefs::kWaitingForMultiProfileForcedMigrationTimestamp),
            base::Time());
}

// Tests that the force-migration pref is *not* recorded for a correctly mapped
// consumer account.
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       ForceMigrationPrefNotRecordedForPersonalAccountInPersonalProfile) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
  EXPECT_EQ(GetApplicationContext()->GetLocalState()->GetTime(
                prefs::kWaitingForMultiProfileForcedMigrationTimestamp),
            base::Time());

  // A consumer identity exists on the device, and is set as the primary account
  // in the personal profile.
  system_identity_manager_->AddIdentity(gmail_identity1);
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce([](ProfileAttributesIOS& attr) {
        attr.SetAuthenticationInfo(
            gmail_identity1.gaiaId,
            base::SysNSStringToUTF8(gmail_identity1.userFullName));
        attr.SetAttachedGaiaIds({gmail_identity1.gaiaId});
      }));

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  EXPECT_THAT(profile_attributes_storage()
                  ->GetAttributesForProfileWithName(kPersonalProfileName)
                  .GetAttachedGaiaIds(),
              UnorderedElementsAre(gmail_identity1.gaiaId));
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  // Verify the force-migration pref is not set.
  EXPECT_EQ(GetApplicationContext()->GetLocalState()->GetTime(
                prefs::kWaitingForMultiProfileForcedMigrationTimestamp),
            base::Time());
}

// Tests that the force-migration pref is *not* recorded for a correctly mapped
// managed account.
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       ForceMigrationPrefNotRecordedForManagedAccountInManagedProfile) {
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
  EXPECT_EQ(GetApplicationContext()->GetLocalState()->GetTime(
                prefs::kWaitingForMultiProfileForcedMigrationTimestamp),
            base::Time());

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get(),
      GetApplicationContext()->GetLocalState());

  // Managed account added after AccountProfileMapper is created.
  system_identity_manager_->AddIdentity(google_identity);

  // A new enterprise profile should've been registered.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);

  // Find the name of the new profile.
  std::string managed_profile_name = FindCreatedProfileName(
      /*known_profile_names=*/{std::string(kPersonalProfileName)});
  ASSERT_FALSE(managed_profile_name.empty());

  // Verify the assignment of the identity to profile.
  NSArray* expected_identities_managed = @[ google_identity ];
  EXPECT_NSEQ(expected_identities_managed,
              GetIdentitiesForProfile(managed_profile_name));

  // Verify the force-migration pref is not set.
  EXPECT_EQ(GetApplicationContext()->GetLocalState()->GetTime(
                prefs::kWaitingForMultiProfileForcedMigrationTimestamp),
            base::Time());
}

}  // namespace

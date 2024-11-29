// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_profile_mapper.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_file_util.h"
#import "base/test/test_future.h"
#import "base/uuid.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using testing::_;

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

class MockObserver : public AccountProfileMapper::Observer {
 public:
  MockObserver() = default;
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  MOCK_METHOD(void, OnIdentityListChanged, (), (override));
  MOCK_METHOD(void, OnIdentityUpdated, (id<SystemIdentity>), (override));
  MOCK_METHOD(void,
              OnIdentityRefreshTokenUpdated,
              (id<SystemIdentity>),
              (override));
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

  void AddObserver(ProfileManagerObserverIOS* observer) override {
    NOTREACHED();
  }
  void RemoveObserver(ProfileManagerObserverIOS* observer) override {
    NOTREACHED();
  }

  ProfileIOS* GetProfileWithName(std::string_view name) override {
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
    return !HasProfileWithName(name);
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
          FROM_HERE, base::BindOnce(std::move(created_callback), profile));
    }
    if (initialized_callback) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(initialized_callback), profile));
    }
    return false;
  }

  ProfileIOS* LoadProfile(std::string_view name) override { NOTREACHED(); }
  ProfileIOS* CreateProfile(std::string_view name) override { NOTREACHED(); }

  void UnloadProfile(std::string_view name) override { NOTREACHED(); }
  void UnloadAllProfiles() override { NOTREACHED(); }

  ProfileAttributesStorageIOS* GetProfileAttributesStorage() override {
    return &profile_attributes_storage_;
  }

 private:
  ProfileAttributesStorageIOS profile_attributes_storage_;

  std::map<std::string, std::unique_ptr<FakeProfileIOS>, std::less<>>
      profiles_map_;
};

class AccountProfileMapperTest : public PlatformTest {
 public:
  explicit AccountProfileMapperTest(bool separate_profiles_enabled) {
    features_.InitWithFeatureState(kSeparateProfilesForManagedAccounts,
                                   separate_profiles_enabled);

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
    for (size_t i = 0; i < storage->GetNumberOfProfiles(); i++) {
      std::string profile_name =
          storage->GetAttributesForProfileAtIndex(i).GetProfileName();
      if (!known_profile_names.contains(profile_name)) {
        return profile_name;
      }
    }
    NOTREACHED();
  }

  ProfileAttributesStorageIOS* profile_attributes_storage() {
    return profile_manager_->GetProfileAttributesStorage();
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::TaskEnvironment task_environment_;
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
      : AccountProfileMapperTest(/*separate_profiles_enabled=*/true) {}
  ~AccountProfileMapperAccountsInSeparateProfilesTest() override = default;
};

class AccountProfileMapperAccountsInSingleProfileTest
    : public AccountProfileMapperTest {
 public:
  AccountProfileMapperAccountsInSingleProfileTest()
      : AccountProfileMapperTest(/*separate_profiles_enabled=*/false) {}
  ~AccountProfileMapperAccountsInSingleProfileTest() override = default;
};

// Tests that AccountProfileMapper lists no identity when there are no
// identities.
TEST_F(AccountProfileMapperAccountsInSingleProfileTest, NoIdentity) {
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_observer;
  account_profile_mapper_->AddObserver(&mock_observer, kPersonalProfileName);

  EXPECT_NSEQ(@[], GetIdentitiesForProfile(kPersonalProfileName));

  account_profile_mapper_->RemoveObserver(&mock_observer, kPersonalProfileName);
}

TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       OnlyAvailableOnIos17Plus) {
  if (@available(iOS 17, *)) {
    EXPECT_TRUE(AreSeparateProfilesForManagedAccountsEnabled());
  } else {
    EXPECT_FALSE(AreSeparateProfilesForManagedAccountsEnabled());
  }
}

TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest, NoIdentity) {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_observer;
  account_profile_mapper_->AddObserver(&mock_observer, kPersonalProfileName);

  EXPECT_NSEQ(@[], GetIdentitiesForProfile(kPersonalProfileName));

  account_profile_mapper_->RemoveObserver(&mock_observer, kPersonalProfileName);
}

// Tests that `OnIdentityRefreshTokenUpdated()` is called when the refresh
// token is updated. This should be done to the observer of the identity.
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       RefreshTokenNotification) {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }
  const std::string kTestProfile1Name("TestProfile1");
  base::test::TestFuture<ProfileIOS*> profile_initialized;
  profile_manager_->CreateProfileAsync(
      kTestProfile1Name, profile_initialized.GetCallback(), base::DoNothing());
  ASSERT_TRUE(profile_initialized.Wait());

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_personal_observer;
  account_profile_mapper_->AddObserver(&mock_personal_observer,
                                       kPersonalProfileName);
  testing::StrictMock<MockObserver> mock_profile1_observer;
  account_profile_mapper_->AddObserver(&mock_profile1_observer,
                                       kTestProfile1Name);

  EXPECT_CALL(mock_personal_observer, OnIdentityListChanged());
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
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_personal_observer;
  account_profile_mapper_->AddObserver(&mock_personal_observer,
                                       kPersonalProfileName);

  EXPECT_CALL(mock_personal_observer, OnIdentityListChanged());
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
  base::test::TestFuture<ProfileIOS*> profile_initialized;
  profile_manager_->CreateProfileAsync(
      kTestProfile1Name, profile_initialized.GetCallback(), base::DoNothing());
  ASSERT_TRUE(profile_initialized.Wait());

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_observer0;
  account_profile_mapper_->AddObserver(&mock_observer0, kPersonalProfileName);
  testing::StrictMock<MockObserver> mock_observer1;
  account_profile_mapper_->AddObserver(&mock_observer1, kTestProfile1Name);

  // Identity events should be forwarded to all observers.
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(3);
  EXPECT_CALL(mock_observer1, OnIdentityListChanged()).Times(3);
  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(gmail_identity2);
  system_identity_manager_->AddIdentity(google_identity);

  EXPECT_CALL(mock_observer0, OnIdentityUpdated(gmail_identity1));
  EXPECT_CALL(mock_observer1, OnIdentityUpdated(gmail_identity1));
  system_identity_manager_->FireIdentityUpdatedNotification(gmail_identity1);

  // All identities should be visible in all profiles.
  NSArray* expected_identities =
      @[ gmail_identity1, gmail_identity2, google_identity ];
  EXPECT_NSEQ(expected_identities,
              GetIdentitiesForProfile(kPersonalProfileName));
  EXPECT_NSEQ(expected_identities, GetIdentitiesForProfile(kTestProfile1Name));

  // Remove an identity; this should also apply to all profiles.
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  EXPECT_CALL(mock_observer1, OnIdentityListChanged()).Times(1);
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
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_observer;
  account_profile_mapper_->AddObserver(&mock_observer, kPersonalProfileName);

  EXPECT_CALL(mock_observer, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer, OnIdentityListChanged()).Times(1);
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
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_observer_personal;
  account_profile_mapper_->AddObserver(&mock_observer_personal,
                                       kPersonalProfileName);

  EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity2);

  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
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

  // Ensure identity events get forwarded (only) to the appropriate observer.
  EXPECT_CALL(mock_observer_personal, OnIdentityUpdated(gmail_identity2));
  system_identity_manager_->FireIdentityUpdatedNotification(gmail_identity2);

  EXPECT_CALL(mock_observer_managed, OnIdentityUpdated(google_identity));
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
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_observer_personal;
  account_profile_mapper_->AddObserver(&mock_observer_personal,
                                       kPersonalProfileName);

  EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity2);

  // Add a managed identity. This should trigger the registration of a new
  // profile.
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
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
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_observer_personal;
  account_profile_mapper_->AddObserver(&mock_observer_personal,
                                       kPersonalProfileName);
  EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity2);

  // Add a managed identity. This should trigger the creation of a new profile.
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
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
    EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
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
    EXPECT_CALL(mock_observer_managed, OnIdentityListChanged()).Times(1);
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
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());

  system_identity_manager_->AddIdentity(gmail_identity1);

  // Add a managed identity, which kicks off the async creation of a profile.
  system_identity_manager_->AddIdentity(google_identity);
  // Before the profile creation completes, do something else which triggers
  // OnIdentityListChanged() and thus (re-)assignment of identities to profiles.
  // This should *not* kick off creation of another profile.
  system_identity_manager_->AddIdentity(gmail_identity2);

  // Exactly one new enterprise profile should've been registered.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 2u);
}

// Tests that the mapping between profiles and accounts is populated when the
// AccountProfileMapper is created, even without any notifications from
// SystemIdentityManager.
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       IdentitiesAreAssignedOnStartup) {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  // Some identities already exist before the AccountProfileMapper is created.
  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(google_identity);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());

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

// Tests that pre-existing identities which are already assigned to a profile
// remain in that profile, even if they'd now be assigned to a different one.
// This is important for managed accounts that pre-date the multi-profile
// support, since those shouldn't be automatically moved into a new profile.
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       DoesNotReassignIdentities) {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  // A consumer identity and a managed identity already exist before the
  // AccountProfileMapper is created.
  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(google_identity);

  // Both identities are already assigned to the personal profile. This is
  // typically the case if the identities were added before multi-profile
  // support was enabled.
  profile_attributes_storage()->UpdateAttributesForProfileWithName(
      kPersonalProfileName, base::BindOnce([](ProfileAttributesIOS attr) {
        attr.SetAttachedGaiaIds(
            {base::SysNSStringToUTF8(gmail_identity1.gaiaID),
             base::SysNSStringToUTF8(google_identity.gaiaID)});
        return attr;
      }));
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());

  // Both identities should still be attached to the personal profile.
  EXPECT_EQ(profile_attributes_storage()
                ->GetAttributesForProfileWithName(kPersonalProfileName)
                .GetAttachedGaiaIds()
                .size(),
            2u);

  // No additional profile should've been registered.
  EXPECT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);
}

// Tests that the personal profile gets correctly converted into a managed
// profile on MakePersonalProfileManagedWithGaiaID(), and a new personal profile
// gets created.
TEST_F(AccountProfileMapperAccountsInSeparateProfilesTest,
       ConvertsPersonalProfileToManaged) {
  // Separate profiles are only available in iOS 17+.
  if (!@available(iOS 17, *)) {
    return;
  }
  ASSERT_EQ(profile_attributes_storage()->GetNumberOfProfiles(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());

  // A personal and a managed account get added.
  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(google_identity);

  // Two profile should be registered, the personal one and a managed one, each
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

  // Simulate that the user signs in with the managed account, and chooses to
  // take existing local data along, i.e. convert the personal profile into a
  // managed profile.
  account_profile_mapper_->MakePersonalProfileManagedWithGaiaID(
      base::SysNSStringToUTF8(google_identity.gaiaID));

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

}  // namespace

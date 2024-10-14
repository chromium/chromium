// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_profile_mapper.h"

#import "base/memory/raw_ptr.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_file_util.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/profile/model/constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
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

const std::string kDefaultProfileName(kIOSChromeInitialProfile);

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

  ProfileIOS* GetOriginalChromeBrowserState() override { NOTREACHED(); }
  ProfileIOS* GetOriginalProfile() override { NOTREACHED(); }
  bool HasOffTheRecordChromeBrowserState() const override { NOTREACHED(); }
  bool HasOffTheRecordProfile() const override { NOTREACHED(); }
  ProfileIOS* GetOffTheRecordChromeBrowserState() override { NOTREACHED(); }
  ProfileIOS* GetOffTheRecordProfile() override { NOTREACHED(); }
  void DestroyOffTheRecordChromeBrowserState() override { NOTREACHED(); }
  void DestroyOffTheRecordProfile() override { NOTREACHED(); }
  BrowserStatePolicyConnector* GetPolicyConnector() override { NOTREACHED(); }
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
    // Load the "Default" profile. This is similar to what the real
    // ProfileManagerIOS does on startup.
    // TestProfileIOS::Builder builder;
    // builder.SetName(kDefaultProfileName);
    // profiles_map_[kDefaultProfileName] = std::move(builder).Build();
    profiles_map_[kDefaultProfileName] =
        std::make_unique<FakeProfileIOS>(kDefaultProfileName);
    profile_attributes_storage_.AddProfile(kDefaultProfileName);
  }
  ~FakeProfileManagerIOS() override = default;

  void AddObserver(ProfileManagerObserverIOS* observer) override {
    NOTREACHED();
  }
  void RemoveObserver(ProfileManagerObserverIOS* observer) override {
    NOTREACHED();
  }

  void LoadProfiles() override { NOTREACHED(); }

  ProfileIOS* GetProfileWithName(std::string_view name) override {
    auto it = profiles_map_.find(name);
    if (it != profiles_map_.end()) {
      return it->second.get();
    }
    return nullptr;
  }

  std::vector<ProfileIOS*> GetLoadedProfiles() override {
    std::vector<ProfileIOS*> profiles;
    for (const auto& [name, profile] : profiles_map_) {
      profiles.push_back(profile.get());
    }
    return profiles;
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

  ProfileAttributesStorageIOS* GetProfileAttributesStorage() override {
    return &profile_attributes_storage_;
  }

 private:
  ProfileAttributesStorageIOS profile_attributes_storage_;

  std::map<std::string, std::unique_ptr<FakeProfileIOS>, std::less<>>
      profiles_map_;
};

}  // namespace

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
    CHECK(profile_manager_->GetProfileWithName(profile_name));

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

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeProfileManagerIOS> profile_manager_;
  raw_ptr<FakeSystemIdentityManager> system_identity_manager_;
  std::unique_ptr<AccountProfileMapper> account_profile_mapper_;
};

// Tests that AccountProfileMapper list no identity when there are no
// identities.
TEST_F(AccountProfileMapperTest, TestWithNoIdentity) {
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_observer;
  account_profile_mapper_->AddObserver(&mock_observer, kDefaultProfileName);
  // Check profile identities and observer.
  NSArray* expected_identities = @[];
  EXPECT_NSEQ(expected_identities,
              GetIdentitiesForProfile(kDefaultProfileName));

  account_profile_mapper_->RemoveObserver(&mock_observer, kDefaultProfileName);
}

// Tests that when the feature flag is disabled, all identities are visible
// in all profiles.
TEST_F(AccountProfileMapperTest, TestWithFlagDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kSeparateProfilesForManagedAccounts);

  const std::string kTestProfile1Name("TestProfile1");
  base::test::TestFuture<ProfileIOS*> profile_initialized;
  profile_manager_->CreateProfileAsync(
      kTestProfile1Name, profile_initialized.GetCallback(), base::DoNothing());
  ASSERT_TRUE(profile_initialized.Wait());

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_observer0;
  account_profile_mapper_->AddObserver(&mock_observer0, kDefaultProfileName);
  testing::StrictMock<MockObserver> mock_observer1;
  account_profile_mapper_->AddObserver(&mock_observer1, kTestProfile1Name);

  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(3);
  EXPECT_CALL(mock_observer1, OnIdentityListChanged()).Times(3);
  system_identity_manager_->AddIdentity(gmail_identity1);
  system_identity_manager_->AddIdentity(gmail_identity2);
  system_identity_manager_->AddIdentity(google_identity);

  EXPECT_CALL(mock_observer0, OnIdentityUpdated(gmail_identity1));
  EXPECT_CALL(mock_observer1, OnIdentityUpdated(gmail_identity1));
  system_identity_manager_->FireIdentityUpdatedNotification(gmail_identity1);

  NSArray* expected_identities =
      @[ gmail_identity1, gmail_identity2, google_identity ];
  EXPECT_NSEQ(expected_identities,
              GetIdentitiesForProfile(kDefaultProfileName));
  EXPECT_NSEQ(expected_identities, GetIdentitiesForProfile(kTestProfile1Name));

  account_profile_mapper_->RemoveObserver(&mock_observer1, kTestProfile1Name);
  account_profile_mapper_->RemoveObserver(&mock_observer0, kDefaultProfileName);
}

// Tests that 2 non-managed identities are added to the personal profile.
TEST_F(AccountProfileMapperTest, TestWithTwoIdentities) {
  base::test::ScopedFeatureList features{kSeparateProfilesForManagedAccounts};

  ASSERT_EQ(profile_manager_->GetLoadedProfiles().size(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_observer;
  account_profile_mapper_->AddObserver(&mock_observer, kDefaultProfileName);
  EXPECT_CALL(mock_observer, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity2);
  // Check profile identities and observer.
  NSArray* expected_identities0 = @[ gmail_identity1, gmail_identity2 ];
  EXPECT_NSEQ(expected_identities0,
              GetIdentitiesForProfile(kDefaultProfileName));
  // Check that no other profiles have been created.
  EXPECT_EQ(profile_manager_->GetLoadedProfiles().size(), 1u);

  account_profile_mapper_->RemoveObserver(&mock_observer, kDefaultProfileName);
}

// Tests that the 2 non-managed identities are added in the personal profile,
// and the managed identity is added to a newly-created separate profile.
TEST_F(AccountProfileMapperTest, TestWithTwoIdentitiesOneManaged) {
  base::test::ScopedFeatureList features{kSeparateProfilesForManagedAccounts};

  ASSERT_EQ(profile_manager_->GetLoadedProfiles().size(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_observer_personal;
  account_profile_mapper_->AddObserver(&mock_observer_personal,
                                       kDefaultProfileName);

  EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity2);

  system_identity_manager_->AddIdentity(google_identity);
  // Wait for the enterprise profile to get created.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(profile_manager_->GetLoadedProfiles().size(), 2u);
  std::string managed_profile_name;
  for (const ProfileIOS* profile : profile_manager_->GetLoadedProfiles()) {
    if (profile->GetProfileName() != kDefaultProfileName) {
      managed_profile_name = profile->GetProfileName();
      break;
    }
  }
  ASSERT_FALSE(managed_profile_name.empty());

  testing::StrictMock<MockObserver> mock_observer_managed;
  account_profile_mapper_->AddObserver(&mock_observer_managed,
                                       managed_profile_name);

  EXPECT_CALL(mock_observer_personal, OnIdentityUpdated(gmail_identity2));
  system_identity_manager_->FireIdentityUpdatedNotification(gmail_identity2);

  EXPECT_CALL(mock_observer_managed, OnIdentityUpdated(google_identity));
  system_identity_manager_->FireIdentityUpdatedNotification(google_identity);

  // Check personal profile identities and observer.
  NSArray* expected_identities0 = @[ gmail_identity1, gmail_identity2 ];
  EXPECT_NSEQ(expected_identities0,
              GetIdentitiesForProfile(kDefaultProfileName));
  // Check managed profile identities and observer.
  NSArray* expected_identities_managed = @[ google_identity ];
  EXPECT_NSEQ(expected_identities_managed,
              GetIdentitiesForProfile(managed_profile_name));

  account_profile_mapper_->RemoveObserver(&mock_observer_personal,
                                          kDefaultProfileName);
  account_profile_mapper_->RemoveObserver(&mock_observer_managed,
                                          managed_profile_name);
}

// Tests that the 2 non-managed identity are added in the personal profile.
// Tests that the first managed identity is added to a newly-created profile,
// and the second managed identity is added to a *different* newly-created
// profile.
TEST_F(AccountProfileMapperTest, TestWithTwoIdentitiesTwoManaged) {
  base::test::ScopedFeatureList features{kSeparateProfilesForManagedAccounts};

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_observer_personal;
  account_profile_mapper_->AddObserver(&mock_observer_personal,
                                       kDefaultProfileName);

  EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity2);

  system_identity_manager_->AddIdentity(google_identity);
  // Wait for the enterprise profile to get created.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(profile_manager_->GetLoadedProfiles().size(), 2u);
  std::string managed_profile_name1;
  for (const ProfileIOS* profile : profile_manager_->GetLoadedProfiles()) {
    if (profile->GetProfileName() != kDefaultProfileName) {
      managed_profile_name1 = profile->GetProfileName();
      break;
    }
  }
  ASSERT_FALSE(managed_profile_name1.empty());

  testing::StrictMock<MockObserver> mock_observer_managed1;
  account_profile_mapper_->AddObserver(&mock_observer_managed1,
                                       managed_profile_name1);

  system_identity_manager_->AddIdentity(chromium_identity);
  // Wait for the enterprise profile to get created.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(profile_manager_->GetLoadedProfiles().size(), 3u);
  std::string managed_profile_name2;
  for (const ProfileIOS* profile : profile_manager_->GetLoadedProfiles()) {
    if (profile->GetProfileName() != kDefaultProfileName &&
        profile->GetProfileName() != managed_profile_name1) {
      managed_profile_name2 = profile->GetProfileName();
      break;
    }
  }
  ASSERT_FALSE(managed_profile_name2.empty());

  testing::StrictMock<MockObserver> mock_observer_managed2;
  account_profile_mapper_->AddObserver(&mock_observer_managed2,
                                       managed_profile_name2);

  // Check personal profile identities and observer.
  NSArray* expected_identities_personal = @[ gmail_identity1, gmail_identity2 ];
  EXPECT_NSEQ(expected_identities_personal,
              GetIdentitiesForProfile(kDefaultProfileName));
  // Check first managed profile identities and observer.
  NSArray* expected_identities_managed1 = @[ google_identity ];
  EXPECT_NSEQ(expected_identities_managed1,
              GetIdentitiesForProfile(managed_profile_name1));
  // Check second managed profile identities and observer.
  NSArray* expected_identities_managed2 = @[ chromium_identity ];
  EXPECT_NSEQ(expected_identities_managed2,
              GetIdentitiesForProfile(managed_profile_name2));

  account_profile_mapper_->RemoveObserver(&mock_observer_personal,
                                          kDefaultProfileName);
  account_profile_mapper_->RemoveObserver(&mock_observer_managed1,
                                          managed_profile_name1);
  account_profile_mapper_->RemoveObserver(&mock_observer_managed2,
                                          managed_profile_name2);
}

// Tests that an identity is removed correctly from the personal profile.
// And tests that an managed identity is removed correctly from its profile.
TEST_F(AccountProfileMapperTest, TestRemoveIdentity) {
  base::test::ScopedFeatureList features{kSeparateProfilesForManagedAccounts};

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());
  testing::StrictMock<MockObserver> mock_observer_personal;
  account_profile_mapper_->AddObserver(&mock_observer_personal,
                                       kDefaultProfileName);
  EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity2);

  system_identity_manager_->AddIdentity(google_identity);
  // Wait for the enterprise profile to get created.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(profile_manager_->GetLoadedProfiles().size(), 2u);
  std::string managed_profile_name;
  for (const ProfileIOS* profile : profile_manager_->GetLoadedProfiles()) {
    if (profile->GetProfileName() != kDefaultProfileName) {
      managed_profile_name = profile->GetProfileName();
      break;
    }
  }
  ASSERT_FALSE(managed_profile_name.empty());

  testing::StrictMock<MockObserver> mock_observer_managed;
  account_profile_mapper_->AddObserver(&mock_observer_managed,
                                       managed_profile_name);

  // Remove a personal identity.
  EXPECT_CALL(mock_observer_personal, OnIdentityListChanged()).Times(1);
  base::RunLoop run_loop;
  auto forget_callback = base::BindOnce(
      [](base::RunLoop* run_loop, NSError* error) {
        EXPECT_EQ(nil, error);
        run_loop->Quit();
      },
      &run_loop);
  system_identity_manager_->ForgetIdentity(gmail_identity2,
                                           std::move(forget_callback));
  run_loop.Run();
  // Check personal profile identities and observer.
  NSArray* expected_identities_personal = @[ gmail_identity1 ];
  EXPECT_NSEQ(expected_identities_personal,
              GetIdentitiesForProfile(kDefaultProfileName));
  // Check managed profile identities and observer.
  NSArray* expected_identities_managed = @[ google_identity ];
  EXPECT_NSEQ(expected_identities_managed,
              GetIdentitiesForProfile(managed_profile_name));

  // Remove the managed identity.
  EXPECT_CALL(mock_observer_managed, OnIdentityListChanged()).Times(1);
  system_identity_manager_->ForgetIdentity(google_identity, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  // Check personal profile identities and observer.
  expected_identities_personal = @[ gmail_identity1 ];
  EXPECT_NSEQ(expected_identities_personal,
              GetIdentitiesForProfile(kDefaultProfileName));
  // Check managed profile identities and observer.
  // TODO(crbug.com/331783685): The managed profile should get deleted here.
  expected_identities_managed = @[];
  EXPECT_NSEQ(expected_identities_managed,
              GetIdentitiesForProfile(managed_profile_name));

  account_profile_mapper_->RemoveObserver(&mock_observer_personal,
                                          kDefaultProfileName);
  account_profile_mapper_->RemoveObserver(&mock_observer_managed,
                                          managed_profile_name);
}

// Tests that only a single profile is created for a managed identity.
TEST_F(AccountProfileMapperTest, TestOnlyOneProfilePerIdentity) {
  base::test::ScopedFeatureList features{kSeparateProfilesForManagedAccounts};

  ASSERT_EQ(profile_manager_->GetLoadedProfiles().size(), 1u);

  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_manager_.get());

  system_identity_manager_->AddIdentity(gmail_identity1);

  // Add a managed identity, which kicks off the async creation of a profile.
  system_identity_manager_->AddIdentity(google_identity);
  // Before the profile creation completes, do something else which triggers
  // OnIdentityListChanged() and thus (re-)assignment of identities to profiles.
  // This should *not* kick off creation of another profile.
  system_identity_manager_->AddIdentity(gmail_identity2);

  // Wait for the enterprise profile to get created.
  task_environment_.RunUntilIdle();

  // Ensure that only a single profile was created for the managed identity.
  EXPECT_EQ(profile_manager_->GetLoadedProfiles().size(), 2u);
}

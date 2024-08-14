// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_profile_mapper.h"

#import "base/memory/raw_ptr.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using testing::_;

namespace {

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

  MOCK_METHOD0(OnIdentityListChanged, void());
};

}  // namespace

class AccountProfileMapperTest : public PlatformTest {
 public:
  AccountProfileMapperTest() {
    TestChromeBrowserState::Builder builder;
    browser_state_ = std::move(builder).Build();

    system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
  }

  ~AccountProfileMapperTest() override {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:experimental_flags::kDisplaySwitchProfile];
  }

  [[nodiscard]] NSArray* GetIdentitiesForProfile(size_t profile_index) {
    NSMutableArray* identities = [NSMutableArray array];
    auto callback = base::BindRepeating(
        [](NSMutableArray* identities, id<SystemIdentity> identity) {
          EXPECT_FALSE([identities containsObject:identity]);
          [identities addObject:identity];
          return AccountProfileMapper::IteratorResult::kContinueIteration;
        },
        identities);
    account_profile_mapper_->IterateOverIdentities(callback, profile_index);
    return identities;
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  raw_ptr<FakeSystemIdentityManager> system_identity_manager_;
  std::unique_ptr<AccountProfileMapper> account_profile_mapper_;
};

// Tests that AccountProfileMapper list no identity when there is no identity
// and one profile.
TEST_F(AccountProfileMapperTest, TestWithNoIdentity) {
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, /*profile_count=*/1);
  testing::StrictMock<MockObserver> mock_observer0;
  account_profile_mapper_->AddObserver(&mock_observer0, /*profile_index=*/0);
  // Check profile identities and observer.
  NSArray* expected_identities = @[];
  EXPECT_NSEQ(expected_identities,
              GetIdentitiesForProfile(/*profile_index=*/0));

  account_profile_mapper_->RemoveObserver(&mock_observer0, /*profile_index=*/0);
}

// Tests that all 3 identities are listed in the only profile.
TEST_F(AccountProfileMapperTest, TestWithThreeIdentitiesOneProfile) {
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, /*profile_count=*/1);
  testing::StrictMock<MockObserver> mock_observer0;
  account_profile_mapper_->AddObserver(&mock_observer0, /*profile_index=*/0);
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity2);
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(google_identity);
  // Check profile identities and observer.
  NSArray* expected_identities =
      @[ gmail_identity1, gmail_identity2, google_identity ];
  EXPECT_NSEQ(expected_identities,
              GetIdentitiesForProfile(/*profile_index=*/0));

  account_profile_mapper_->RemoveObserver(&mock_observer0, /*profile_index=*/0);
}

// Tests that 2 non managed identities are added to the personal profile.
TEST_F(AccountProfileMapperTest, TestWithTwoIdentitiesTwoProfiles) {
  const size_t profile_count = 2;
  [[NSUserDefaults standardUserDefaults]
      setInteger:profile_count
          forKey:experimental_flags::kDisplaySwitchProfile];
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_count);
  testing::StrictMock<MockObserver> mock_observer0;
  account_profile_mapper_->AddObserver(&mock_observer0, /*profile_index=*/0);
  testing::StrictMock<MockObserver> mock_observer1;
  account_profile_mapper_->AddObserver(&mock_observer1, /*profile_index=*/1);
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity2);
  // Check #0 profile identities and observer.
  NSArray* expected_identities0 = @[ gmail_identity1, gmail_identity2 ];
  EXPECT_NSEQ(expected_identities0,
              GetIdentitiesForProfile(/*profile_index=*/0));
  // Check #1 profile identities and observer.
  NSArray* expected_identities1 = @[];
  EXPECT_NSEQ(expected_identities1,
              GetIdentitiesForProfile(/*profile_index=*/1));
  //  EXPECT_EQ(mock_observer1.on_identity_list_changed_called_count, 0);

  account_profile_mapper_->RemoveObserver(&mock_observer0, /*profile_index=*/0);
  account_profile_mapper_->RemoveObserver(&mock_observer1, /*profile_index=*/1);
}

// Tests that the 2 non managed identity are added in the personal profile,
// and the managed identity is added to the other profile.
TEST_F(AccountProfileMapperTest, TestWithTwoIdentitiesOneManagedTwoProfiles) {
  const size_t profile_count = 2;
  [[NSUserDefaults standardUserDefaults]
      setInteger:profile_count
          forKey:experimental_flags::kDisplaySwitchProfile];
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_count);
  testing::StrictMock<MockObserver> mock_observer0;
  account_profile_mapper_->AddObserver(&mock_observer0, /*profile_index=*/0);
  testing::StrictMock<MockObserver> mock_observer1;
  account_profile_mapper_->AddObserver(&mock_observer1, /*profile_index=*/1);
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity2);
  EXPECT_CALL(mock_observer1, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(google_identity);
  // Check #0 profile identities and observer.
  NSArray* expected_identities0 = @[ gmail_identity1, gmail_identity2 ];
  EXPECT_NSEQ(expected_identities0,
              GetIdentitiesForProfile(/*profile_index=*/0));
  // Check #1 profile identities and observer.
  NSArray* expected_identities1 = @[ google_identity ];
  EXPECT_NSEQ(expected_identities1,
              GetIdentitiesForProfile(/*profile_index=*/1));

  account_profile_mapper_->RemoveObserver(&mock_observer0, /*profile_index=*/0);
  account_profile_mapper_->RemoveObserver(&mock_observer1, /*profile_index=*/1);
}

// Tests that the 2 non managed identity are added in the personal profile.
// Tests that the first managed identity is added to the other profile.
// Tests that the second managed identity is added to the personal profile
// since there is no extra profile available.
// TODO(crbug.com/331783685): This test needs to be updated when
// AccountProfileMapper is abled to create profiles.
TEST_F(AccountProfileMapperTest, TestWithTwoIdentitiesTwoManagedTwoProfiles) {
  const size_t profile_count = 2;
  [[NSUserDefaults standardUserDefaults]
      setInteger:profile_count
          forKey:experimental_flags::kDisplaySwitchProfile];
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_count);
  testing::StrictMock<MockObserver> mock_observer0;
  account_profile_mapper_->AddObserver(&mock_observer0, /*profile_index=*/0);
  testing::StrictMock<MockObserver> mock_observer1;
  account_profile_mapper_->AddObserver(&mock_observer1, /*profile_index=*/1);
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity2);
  EXPECT_CALL(mock_observer1, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(google_identity);
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(chromium_identity);
  // Check #0 profile identities and observer.
  NSArray* expected_identities0 =
      @[ gmail_identity1, gmail_identity2, chromium_identity ];
  EXPECT_NSEQ(expected_identities0,
              GetIdentitiesForProfile(/*profile_index=*/0));
  // Check #1 profile identities and observer.
  NSArray* expected_identities1 = @[ google_identity ];
  EXPECT_NSEQ(expected_identities1,
              GetIdentitiesForProfile(/*profile_index=*/1));

  account_profile_mapper_->RemoveObserver(&mock_observer0, /*profile_index=*/0);
  account_profile_mapper_->RemoveObserver(&mock_observer1, /*profile_index=*/1);
}

// Tests that an identity is removed correctly from the personal profile.
// And tests that an managed identity is removed correctly from its profile.
TEST_F(AccountProfileMapperTest, TestRemoveIdentity) {
  const size_t profile_count = 2;
  [[NSUserDefaults standardUserDefaults]
      setInteger:profile_count
          forKey:experimental_flags::kDisplaySwitchProfile];
  account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
      system_identity_manager_, profile_count);
  testing::StrictMock<MockObserver> mock_observer0;
  account_profile_mapper_->AddObserver(&mock_observer0, /*profile_index=*/0);
  testing::StrictMock<MockObserver> mock_observer1;
  account_profile_mapper_->AddObserver(&mock_observer1, /*profile_index=*/1);
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity1);
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(gmail_identity2);
  EXPECT_CALL(mock_observer1, OnIdentityListChanged()).Times(1);
  system_identity_manager_->AddIdentity(google_identity);

  // Remove an identity.
  EXPECT_CALL(mock_observer0, OnIdentityListChanged()).Times(1);
  base::RunLoop run_loop;
  auto forget_callback = base::BindOnce(
      [](base::RunLoop* run_loop, NSError* error) {
        EXPECT_EQ(nil, error);
        run_loop->QuitClosure().Run();
      },
      &run_loop);
  system_identity_manager_->ForgetIdentity(gmail_identity2,
                                           std::move(forget_callback));
  run_loop.Run();
  // Check #0 profile identities and observer.
  NSArray* expected_identities0 = @[ gmail_identity1 ];
  EXPECT_NSEQ(expected_identities0,
              GetIdentitiesForProfile(/*profile_index=*/0));
  // Check #1 profile identities and observer.
  NSArray* expected_identities1 = @[ google_identity ];
  EXPECT_NSEQ(expected_identities1,
              GetIdentitiesForProfile(/*profile_index=*/1));

  // Remove the managed identity.
  EXPECT_CALL(mock_observer1, OnIdentityListChanged()).Times(1);
  system_identity_manager_->ForgetIdentity(google_identity, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  // Check #0 profile identities and observer.
  expected_identities0 = @[ gmail_identity1 ];
  EXPECT_NSEQ(expected_identities0,
              GetIdentitiesForProfile(/*profile_index=*/0));
  // Check #1 profile identities and observer.
  expected_identities1 = @[];
  EXPECT_NSEQ(expected_identities1,
              GetIdentitiesForProfile(/*profile_index=*/1));

  account_profile_mapper_->RemoveObserver(&mock_observer0, /*profile_index=*/0);
  account_profile_mapper_->RemoveObserver(&mock_observer1, /*profile_index=*/1);
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/user_population_helper.h"

#import <memory>
#import <string>

#import "base/functional/bind.h"
#import "base/strings/strcat.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/sync/base/data_type.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/test/test_sync_service.h"
#import "components/unified_consent/pref_names.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr char kTestEmail[] = "test@example.com";

using UserPopulationHelperTest = PlatformTest;

// Tests that `number_of_profiles` and `number_of_loaded_profiles` are unset
// when `GetProfileManager` returns null.
TEST_F(UserPopulationHelperTest, StandaloneProfileWithoutProfileManager) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  ASSERT_EQ(GetApplicationContext()->GetProfileManager(), nullptr);
  safe_browsing::ChromeUserPopulation population =
      GetUserPopulationForProfile(profile.get());
  EXPECT_FALSE(population.has_number_of_profiles());
  EXPECT_FALSE(population.has_number_of_loaded_profiles());
}

// Tests that `number_of_profiles` and `number_of_loaded_profiles` reflect
// loaded, reserved unloaded, and deleted profiles, and exclude off-the-record
// profiles.
TEST_F(UserPopulationHelperTest, PopulatesProfileCounts) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestProfileManagerIOS profile_manager;

  TestProfileIOS* profile =
      profile_manager.AddProfileWithBuilder(TestProfileIOS::Builder());
  ASSERT_TRUE(profile);

  // A single loaded profile counts toward both total and loaded profiles.
  safe_browsing::ChromeUserPopulation population =
      GetUserPopulationForProfile(profile);
  EXPECT_TRUE(population.has_number_of_profiles());
  EXPECT_EQ(population.number_of_profiles(), 1);
  EXPECT_TRUE(population.has_number_of_loaded_profiles());
  EXPECT_EQ(population.number_of_loaded_profiles(), 1);

  // A reserved but unloaded profile increments only `number_of_profiles`.
  const std::string unloaded_name = profile_manager.ReserveNewProfileName();
  population = GetUserPopulationForProfile(profile);
  EXPECT_TRUE(population.has_number_of_profiles());
  EXPECT_EQ(population.number_of_profiles(), 2);
  EXPECT_TRUE(population.has_number_of_loaded_profiles());
  EXPECT_EQ(population.number_of_loaded_profiles(), 1);

  // Profiles marked for deletion are excluded from `number_of_profiles`.
  profile_manager.MarkProfileForDeletion(unloaded_name);
  population = GetUserPopulationForProfile(profile);
  EXPECT_TRUE(population.has_number_of_profiles());
  EXPECT_EQ(population.number_of_profiles(), 1);
  EXPECT_TRUE(population.has_number_of_loaded_profiles());
  EXPECT_EQ(population.number_of_loaded_profiles(), 1);

  // Off-the-record profiles are owned by their original profile rather than
  // `ProfileManagerIOS`, so they are excluded from both counts.
  ProfileIOS* incognito_profile = profile->GetOffTheRecordProfile();
  ASSERT_TRUE(incognito_profile);
  population = GetUserPopulationForProfile(incognito_profile);
  EXPECT_TRUE(population.has_number_of_profiles());
  EXPECT_EQ(population.number_of_profiles(), 1);
  EXPECT_TRUE(population.has_number_of_loaded_profiles());
  EXPECT_EQ(population.number_of_loaded_profiles(), 1);
}

// Tests that `number_of_profiles` and `number_of_loaded_profiles` correctly
// reflect a profile transitioning from unloaded to loaded.
TEST_F(UserPopulationHelperTest,
       PopulatesProfileCountsWhenProfileTransitionsFromUnloadedToLoaded) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestProfileManagerIOS profile_manager;

  TestProfileIOS* profile =
      profile_manager.AddProfileWithBuilder(TestProfileIOS::Builder());
  ASSERT_TRUE(profile);

  // Initially, only the personal profile exists and is loaded.
  safe_browsing::ChromeUserPopulation population =
      GetUserPopulationForProfile(profile);
  EXPECT_TRUE(population.has_number_of_profiles());
  EXPECT_EQ(population.number_of_profiles(), 1);
  EXPECT_TRUE(population.has_number_of_loaded_profiles());
  EXPECT_EQ(population.number_of_loaded_profiles(), 1);

  // Registering a managed identity reserves a second profile in
  // `ProfileAttributesStorageIOS` that is unloaded.
  FakeSystemIdentity* managed_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(managed_identity);

  population = GetUserPopulationForProfile(profile);
  EXPECT_TRUE(population.has_number_of_profiles());
  EXPECT_EQ(population.number_of_profiles(), 2);
  EXPECT_TRUE(population.has_number_of_loaded_profiles());
  EXPECT_EQ(population.number_of_loaded_profiles(), 1);

  // Loading the reserved managed profile keeps the total profile count at 2 and
  // increments the loaded profile count to 2.
  TestProfileIOS* managed_profile =
      profile_manager.AddManagedProfileForGaiaIDWithBuilder(
          managed_identity.gaiaId, TestProfileIOS::Builder());
  ASSERT_TRUE(managed_profile);

  population = GetUserPopulationForProfile(profile);
  EXPECT_TRUE(population.has_number_of_profiles());
  EXPECT_EQ(population.number_of_profiles(), 2);
  EXPECT_TRUE(population.has_number_of_loaded_profiles());
  EXPECT_EQ(population.number_of_loaded_profiles(), 2);

  safe_browsing::ChromeUserPopulation managed_population =
      GetUserPopulationForProfile(managed_profile);
  EXPECT_TRUE(managed_population.has_number_of_profiles());
  EXPECT_EQ(managed_population.number_of_profiles(), 2);
  EXPECT_TRUE(managed_population.has_number_of_loaded_profiles());
  EXPECT_EQ(managed_population.number_of_loaded_profiles(), 2);
}

// Tests that `is_incognito` reflects whether the profile is off-the-record.
TEST_F(UserPopulationHelperTest, PopulatesIncognito) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  EXPECT_FALSE(GetUserPopulationForProfile(profile.get()).is_incognito());

  ProfileIOS* incognito_profile = profile->GetOffTheRecordProfile();
  ASSERT_TRUE(incognito_profile);
  EXPECT_TRUE(GetUserPopulationForProfile(incognito_profile).is_incognito());
}

// Tests that `user_population` reflects Standard Protection, Enhanced
// Protection, and disabled Safe Browsing states.
TEST_F(UserPopulationHelperTest, PopulatesSafeBrowsingState) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  safe_browsing::SetSafeBrowsingState(
      profile->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  safe_browsing::ChromeUserPopulation population =
      GetUserPopulationForProfile(profile.get());
  EXPECT_EQ(population.user_population(),
            safe_browsing::ChromeUserPopulation::SAFE_BROWSING);

  safe_browsing::SetSafeBrowsingState(
      profile->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  population = GetUserPopulationForProfile(profile.get());
  EXPECT_EQ(population.user_population(),
            safe_browsing::ChromeUserPopulation::ENHANCED_PROTECTION);

  safe_browsing::SetSafeBrowsingState(
      profile->GetPrefs(), safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);
  population = GetUserPopulationForProfile(profile.get());
  EXPECT_EQ(population.user_population(),
            safe_browsing::ChromeUserPopulation::UNKNOWN_USER_POPULATION);
}

// Tests that `is_mbb_enabled` reflects the Make Searches and Browsing Better
// preference.
TEST_F(UserPopulationHelperTest, PopulatesMBB) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  profile->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  safe_browsing::ChromeUserPopulation population =
      GetUserPopulationForProfile(profile.get());
  EXPECT_FALSE(population.is_mbb_enabled());

  profile->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  population = GetUserPopulationForProfile(profile.get());
  EXPECT_TRUE(population.is_mbb_enabled());
}

// Tests that `is_signed_in` reflects whether the primary account is signed in.
TEST_F(UserPopulationHelperTest, PopulatesSignedIn) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      IdentityManagerFactory::GetInstance(),
      base::BindRepeating(&IdentityTestEnvironmentBrowserStateAdaptor::
                              BuildIdentityManagerForTests));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();

  safe_browsing::ChromeUserPopulation population =
      GetUserPopulationForProfile(profile.get());
  EXPECT_FALSE(population.is_signed_in());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile.get());
  signin::MakePrimaryAccountAvailable(identity_manager, kTestEmail,
                                      signin::ConsentLevel::kSignin);
  population = GetUserPopulationForProfile(profile.get());
  EXPECT_TRUE(population.is_signed_in());
}

// Tests that `is_history_sync_enabled` reflects active history sync state.
TEST_F(UserPopulationHelperTest, PopulatesSync) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                            base::BindRepeating(&CreateTestSyncService));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();

  syncer::TestSyncService* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile.get()));
  ASSERT_TRUE(sync_service->GetActiveDataTypes().Has(
      syncer::HISTORY_DELETE_DIRECTIVES));
  safe_browsing::ChromeUserPopulation population =
      GetUserPopulationForProfile(profile.get());
  EXPECT_TRUE(population.is_history_sync_enabled());

  sync_service->SetSignedOut();
  population = GetUserPopulationForProfile(profile.get());
  EXPECT_FALSE(population.is_history_sync_enabled());

  sync_service->SetLocalSyncEnabled(true);
  population = GetUserPopulationForProfile(profile.get());
  EXPECT_FALSE(population.is_history_sync_enabled());

  sync_service->SetLocalSyncEnabled(false);
  sync_service->SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());
  population = GetUserPopulationForProfile(profile.get());
  EXPECT_FALSE(population.is_history_sync_enabled());
}

// Tests that `user_agent` is populated with the product name, version, and OS.
TEST_F(UserPopulationHelperTest, PopulatesUserAgent) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  const std::string expected_user_agent =
      base::StrCat({version_info::GetProductNameAndVersionForUserAgent(), "/",
                    version_info::GetOSType()});
  safe_browsing::ChromeUserPopulation population =
      GetUserPopulationForProfile(profile.get());
  EXPECT_EQ(population.user_agent(), expected_user_agent);
}

}  // namespace

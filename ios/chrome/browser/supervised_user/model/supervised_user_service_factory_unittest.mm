// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"

#import "base/memory/scoped_refptr.h"
#import "base/threading/thread_restrictions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_notifier_impl.h"
#import "components/prefs/pref_value_store.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

// Marks the First Run has as completed.
// See `ChromeEarlGreyAppInterface::WriteFirstRunSentinel`.
void WriteFirstRunSentinel() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  FirstRun::RemoveSentinel();
  base::File::Error file_error;
  startup_metric_utils::FirstRunSentinelCreationResult sentinel_created =
      FirstRun::CreateSentinel(&file_error);
  ASSERT_EQ(sentinel_created,
            startup_metric_utils::FirstRunSentinelCreationResult::kSuccess);
  FirstRun::LoadSentinelInfo();
  FirstRun::ClearStateForTesting();
  FirstRun::IsChromeFirstRun();
}

// Removes the file that marks a run as a First Run.
// See `ChromeEarlGreyAppInterface::RemoveFirstRunSentinel`.
void RemoveFirstRunSentinel() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (FirstRun::RemoveSentinel()) {
    FirstRun::LoadSentinelInfo();
    FirstRun::ClearStateForTesting();
    FirstRun::IsChromeFirstRun();
  }
}

}  // namespace

// Test fixture for testing SupervisedUserServiceFactory class.
class SupervisedUserServiceFactoryTest : public PlatformTest {
 public:
  SupervisedUserServiceFactoryTest() {
    profile_ = TestProfileIOS::Builder().Build();
    profile_->CreateOffTheRecordBrowserStateWithTestingFactories();
  }

  ProfileIOS* GetRegularProfile() { return profile_.get(); }

  ProfileIOS* GetOffTheRecordProfile() {
    return profile_->GetOffTheRecordProfile();
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that SupervisedUserServiceFactory creates
// SupervisedUserService.
TEST_F(SupervisedUserServiceFactoryTest, CreateService) {
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(GetRegularProfile());
  EXPECT_TRUE(service);
}

// Tests that SupervisedUserServiceFactory retuns null
// with an off-the-record Profile.
TEST_F(SupervisedUserServiceFactoryTest, ReturnsNullOnOffTheRecordProfile) {
  ProfileIOS* otr_profile = GetOffTheRecordProfile();
  ASSERT_TRUE(otr_profile);
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(otr_profile);
  EXPECT_FALSE(service);
}

// Tests that the SU interstitial first time banner should not be created for
// first runs.
TEST_F(SupervisedUserServiceFactoryTest,
       ServiceDoesNotShowFirstTimeInterstitialBannerOnFirstRun) {
  RemoveFirstRunSentinel();

  EXPECT_FALSE(supervised_user::ShouldShowFirstTimeBanner(GetRegularProfile()));
}

// Tests that the SU interstitial first time banner should be created for
// existing users i.e. for an existing pref store.
TEST_F(SupervisedUserServiceFactoryTest,
       ServiceCanShowFirstTimeInterstitialBannerOnExistingUser) {
  WriteFirstRunSentinel();

  EXPECT_TRUE(supervised_user::ShouldShowFirstTimeBanner(GetRegularProfile()));
}

// Tests that the SU interstitial first time banner should not be created for
// new users i.e. for a new pref store.
TEST_F(SupervisedUserServiceFactoryTest,
       ServiceDoesNotShowFirstTimeInterstitialBannerOnNewUser) {
  WriteFirstRunSentinel();

  // Mark the user pref store as new.
  auto user_prefs = base::MakeRefCounted<TestingPrefStore>();
  user_prefs->set_read_error(
      PersistentPrefStore::PrefReadError::PREF_READ_ERROR_NO_FILE);

  auto testing_prefs =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>(
          /*managed_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*supervised_user_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*extension_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*standalone_browser_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          /*user_prefs=*/user_prefs,
          /*recommended_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
          base::MakeRefCounted<user_prefs::PrefRegistrySyncable>(),
          std::make_unique<PrefNotifierImpl>());
  RegisterProfilePrefs(testing_prefs->registry());

  TestProfileIOS::Builder builder;
  builder.SetPrefService(std::move(testing_prefs));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();

  EXPECT_FALSE(supervised_user::ShouldShowFirstTimeBanner(profile.get()));
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"

#import "base/threading/thread_restrictions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_notifier_impl.h"
#import "components/prefs/pref_value_store.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

// Marks the First Run has as completed.
// See `ChromeEarlGreyAppInterface::writeFirstRunSentinel`.
void writeFirstRunSentinel() {
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
// See `ChromeEarlGreyAppInterface::removeFirstRunSentinel`.
void removeFirstRunSentinel() {
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
 protected:
  SupervisedUserServiceFactoryTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}

  // ChromeBrowserState needs thread.
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests that SupervisedUserServiceFactory creates
// SupervisedUserService.
TEST_F(SupervisedUserServiceFactoryTest, CreateService) {
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForBrowserState(browser_state_.get());
  ASSERT_TRUE(service);
}

// Tests that SupervisedUserServiceFactory retuns null
// with an off-the-record ChromeBrowserState.
TEST_F(SupervisedUserServiceFactoryTest,
       ReturnsNullOnOffTheRecordBrowserState) {
  ChromeBrowserState* otr_browser_state =
      browser_state_->CreateOffTheRecordBrowserStateWithTestingFactories({});
  CHECK(otr_browser_state);
  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForBrowserState(otr_browser_state);
  ASSERT_FALSE(service);
}

// Tests that the SU interstitial first time banner should not be created for
// first runs.
TEST_F(SupervisedUserServiceFactoryTest,
       ServiceDoesNotShowFirstTimeInterstitialBannerOnFirstRun) {
  removeFirstRunSentinel();

  ASSERT_FALSE(
      supervised_user::ShouldShowFirstTimeBanner(browser_state_.get()));
}

// Tests that the SU interstitial first time banner should be created for
// existing users i.e. for an existing pref store.
TEST_F(SupervisedUserServiceFactoryTest,
       ServiceCanShowFirstTimeInterstitialBannerOnExistingUser) {
  writeFirstRunSentinel();

  ASSERT_TRUE(supervised_user::ShouldShowFirstTimeBanner(browser_state_.get()));
}

// Tests that the SU interstitial first time banner should not be created for
// new users i.e. for a new pref store.
TEST_F(SupervisedUserServiceFactoryTest,
       ServiceDoesNotShowFirstTimeInterstitialBannerOnNewUser) {
  writeFirstRunSentinel();

  // Mark the user pref store as new.
  auto* user_prefs = new TestingPrefStore();
  user_prefs->set_read_error(
      PersistentPrefStore::PrefReadError::PREF_READ_ERROR_NO_FILE);

  auto testing_prefs =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>(
          /*managed_prefs=*/new TestingPrefStore(),
          /*supervised_user_prefs=*/new TestingPrefStore(),
          /*extension_prefs=*/new TestingPrefStore(),
          /*standalone_browser_prefs=*/new TestingPrefStore(),
          /*user_prefs=*/user_prefs,
          /*recommended_prefs=*/new TestingPrefStore(),
          new user_prefs::PrefRegistrySyncable(), new PrefNotifierImpl());
  RegisterBrowserStatePrefs(testing_prefs->registry());

  TestChromeBrowserState::Builder builder;
  builder.SetPrefService(std::move(testing_prefs));
  std::unique_ptr<TestChromeBrowserState> browser_state = builder.Build();

  ASSERT_FALSE(supervised_user::ShouldShowFirstTimeBanner(browser_state.get()));
}

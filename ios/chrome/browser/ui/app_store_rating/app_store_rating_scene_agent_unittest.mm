// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_store_rating/app_store_rating_scene_agent.h"

#import "base/values.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/mock_promos_manager.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/ui/app_store_rating/constants.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils_test_support.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/test/fake_scene_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::testing::_;
using ::testing::AnyNumber;

// Test fixture for testing AppStoreRatingSceneAgent class.
class AppStoreRatingSceneAgentTest : public PlatformTest {
 protected:
  AppStoreRatingSceneAgentTest() {
    CreateMockPromosManager();
    CreateFakeSceneState();
    CreateAppStoreRatingSceneAgent();
  }

  ~AppStoreRatingSceneAgentTest() override {
    ClearUserDefaults();
    local_state_.Get()->ClearPref(prefs::kAppStoreRatingPolicyEnabled);
  }

 protected:
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  web::WebTaskEnvironment task_environment_;
  AppStoreRatingSceneAgent* test_scene_agent_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  FakeSceneState* fake_scene_state_;

  // Create a MockPromosManager.
  void CreateMockPromosManager() {
    promos_manager_ = std::make_unique<MockPromosManager>();
  }

  // Create a FakeSceneState.
  void CreateFakeSceneState() {
    id mockAppState = OCMClassMock([AppState class]);
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
    fake_scene_state_ =
        [[FakeSceneState alloc] initWithAppState:mockAppState
                                    browserState:browser_state_.get()];
  }

  // Create an AppStoreRatingSceneAgent to test.
  void CreateAppStoreRatingSceneAgent() {
    test_scene_agent_ = [[AppStoreRatingSceneAgent alloc]
        initWithPromosManager:promos_manager_.get()];
    test_scene_agent_.sceneState = fake_scene_state_;
  }

  // Set kAppStoreRatingTotalDaysOnChromeKey in NSUserDefaults.
  void SetTotalDaysOnChrome(int days) {
    [[NSUserDefaults standardUserDefaults]
        setInteger:days
            forKey:kAppStoreRatingTotalDaysOnChromeKey];
  }

  // Set kAppStoreRatingActiveDaysInPastWeekKey in NSUserDefaults to an array of
  // NSDate objects.
  void SetActiveDaysInPastWeek(int activeDays) {
    NSMutableArray* array = [[NSMutableArray alloc] init];
    for (int a = activeDays - 1; a >= 0; a--) {
      [array addObject:CreateDateFromToday(-a)];
    }
    [[NSUserDefaults standardUserDefaults]
        setObject:array
           forKey:kAppStoreRatingActiveDaysInPastWeekKey];
  }

  // Set kAppStoreRatingLastShownPromoDayKey in NSUserDefaults.
  void SetPromoLastShownDaysAgo(int daysAgo) {
    NSDate* date = CreateDateFromToday(-daysAgo);
    [[NSUserDefaults standardUserDefaults]
        setObject:date
           forKey:kAppStoreRatingLastShownPromoDayKey];
  }

  // Remove the keys added to NSUserDefaults.
  void ClearUserDefaults() {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    [defaults removeObjectForKey:kAppStoreRatingActiveDaysInPastWeekKey];
    [defaults removeObjectForKey:kAppStoreRatingTotalDaysOnChromeKey];
    [defaults removeObjectForKey:kAppStoreRatingLastShownPromoDayKey];
    ClearDefaultBrowserPromoData();
  }

  // Ensure that Chrome is considered as default browser.
  void SetTrueChromeLikelyDefaultBrowser() { LogOpenHTTPURLFromExternalURL(); }

  // Ensure that Chrome is not considered as default browser.
  void SetFalseChromeLikelyDefaultBrowser() { ClearDefaultBrowserPromoData(); }

  // Enable Credentials Provider.
  void EnableCPE() {
    browser_state_->GetPrefs()->SetBoolean(
        password_manager::prefs::kCredentialProviderEnabledOnStartup, true);
  }

  // Disable Credentials Provider.
  void DisableCPE() {
    browser_state_->GetPrefs()->SetBoolean(
        password_manager::prefs::kCredentialProviderEnabledOnStartup, false);
  }

  // Helper method that creates an NSDate object.
  NSDate* CreateDateFromToday(int daysToAdd) {
    NSDate* now = [NSDate date];
    NSDateComponents* components = [[NSDateComponents alloc] init];
    [components setDay:daysToAdd];
    NSCalendar* calendar = [NSCalendar currentCalendar];
    NSDate* newDay = [calendar dateByAddingComponents:components
                                               toDate:now
                                              options:0];
    return newDay;
  }
};

#pragma mark - Tests

// Tests that promo display is not requested when the user has
// used Chrome for less than 3 days in the past week, but meets
// the other requirements to be considered engaged.
TEST_F(AppStoreRatingSceneAgentTest, TestChromeNotUsed3DaysInPastWeek) {
  EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(_))
      .Times(0);

  SetActiveDaysInPastWeek(1);
  SetTotalDaysOnChrome(16);
  EnableCPE();
  SetTrueChromeLikelyDefaultBrowser();
  SetPromoLastShownDaysAgo(366);

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is not requested when the user has
// used Chrome for less than 15 days overall, but meets
// the other requirements to be considered engaged.
TEST_F(AppStoreRatingSceneAgentTest, TestChromeNotUsed15Days) {
  EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(_))
      .Times(0);

  SetActiveDaysInPastWeek(3);
  SetTotalDaysOnChrome(10);
  EnableCPE();
  SetTrueChromeLikelyDefaultBrowser();
  SetPromoLastShownDaysAgo(366);

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is not requested when the user has not
// enabled CPE, but meets the other requirements to be considered engaged.
TEST_F(AppStoreRatingSceneAgentTest, TestCPENotEnabled) {
  EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(_))
      .Times(0);

  SetActiveDaysInPastWeek(4);
  SetTotalDaysOnChrome(17);
  DisableCPE();
  SetTrueChromeLikelyDefaultBrowser();
  SetPromoLastShownDaysAgo(366);

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is not requested when the user has not set
// Chrome as their default browser, but meets the other requirements to be
// considered engaged.
TEST_F(AppStoreRatingSceneAgentTest, TestChromeNotDefaultBrowser) {
  EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(_))
      .Times(0);

  SetActiveDaysInPastWeek(7);
  SetTotalDaysOnChrome(15);
  EnableCPE();
  SetFalseChromeLikelyDefaultBrowser();
  SetPromoLastShownDaysAgo(366);

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is not requested when the App Store Rating policy is
// disabled.
TEST_F(AppStoreRatingSceneAgentTest, TestPolicyDisabled) {
  EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(_))
      .Times(0);

  SetActiveDaysInPastWeek(3);
  SetTotalDaysOnChrome(16);
  EnableCPE();
  SetTrueChromeLikelyDefaultBrowser();
  SetPromoLastShownDaysAgo(366);

  // Disabling the policy.
  local_state_.Get()->SetBoolean(prefs::kAppStoreRatingPolicyEnabled, false);

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is not requested when the promo has already been
// registered in the past 365 days.
TEST_F(AppStoreRatingSceneAgentTest, TestPromoRegisteredLessThan365DaysAgo) {
  EXPECT_CALL(*promos_manager_.get(), RegisterPromoForSingleDisplay(_))
      .Times(0);

  SetActiveDaysInPastWeek(3);
  SetTotalDaysOnChrome(15);
  EnableCPE();
  SetTrueChromeLikelyDefaultBrowser();
  SetPromoLastShownDaysAgo(360);

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

// Tests that promo display is requested when the user meets all
// requirements to be considered engaged.
TEST_F(AppStoreRatingSceneAgentTest, TestPromoCorrectlyRequested) {
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForSingleDisplay(promos_manager::Promo::AppStoreRating))
      .Times(1);

  // Creating the requirements to be considered engaged.
  SetActiveDaysInPastWeek(3);
  SetTotalDaysOnChrome(15);
  EnableCPE();
  SetTrueChromeLikelyDefaultBrowser();
  SetPromoLastShownDaysAgo(366);

  // Simulating the user launching or resuming the app.
  [test_scene_agent_ sceneState:fake_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
}

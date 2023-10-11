// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/promo/whats_new_scene_agent.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/mock_promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/whats_new/constants.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

void UpdateWhatsNewDaysAfterFre(int num_day) {
  NSTimeInterval days = num_day * 24 * 60 * 60;
  NSDate* date = [NSDate dateWithTimeIntervalSinceNow:-days];
  [[NSUserDefaults standardUserDefaults] setObject:date
                                            forKey:kWhatsNewDaysAfterFre];
}

void UpdateWhatsNewLaunchesAfterFre(int num_lanches) {
  [[NSUserDefaults standardUserDefaults] setInteger:num_lanches
                                             forKey:kWhatsNewLaunchesAfterFre];
}

void ClearWhatsNewUserData() {
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kWhatsNewDaysAfterFre];
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kWhatsNewLaunchesAfterFre];
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kWhatsNewPromoRegistrationKey];
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kWhatsNewM116PromoRegistrationKey];
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kWhatsNewUsageEntryKey];
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kWhatsNewM116UsageEntryKey];
}

}  // namespace

class WhatsNewSceneAgentTest : public PlatformTest {
 public:
  WhatsNewSceneAgentTest() : PlatformTest() {
    std::unique_ptr<TestChromeBrowserState> browser_state_ =
        TestChromeBrowserState::Builder().Build();
    std::unique_ptr<Browser> browser_ =
        std::make_unique<TestBrowser>(browser_state_.get());
    FakeStartupInformation* startup_information_ =
        [[FakeStartupInformation alloc] init];
    app_state_ =
        [[AppState alloc] initWithStartupInformation:startup_information_];
    promos_manager_ = std::make_unique<MockPromosManager>();
    agent_ = [[WhatsNewSceneAgent alloc]
        initWithPromosManager:promos_manager_.get()];
    scene_state_ =
        [[FakeSceneState alloc] initWithAppState:app_state_
                                    browserState:browser_state_.get()];
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);
    agent_.sceneState = scene_state_;
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
  }

  void TearDown() override { ClearWhatsNewUserData(); }

 protected:
  WhatsNewSceneAgent* agent_;
  // SceneState only weakly holds AppState, so keep it alive here.
  AppState* app_state_;
  FakeSceneState* scene_state_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the What's New promo did not register in the promo manager when
// the conditions aren't met.
TEST_F(WhatsNewSceneAgentTest, TestWhatsNewNoPromoRegistration) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(promos_manager::Promo::WhatsNew))
      .Times(0);

  // Set the number of day after FRE to 0.
  UpdateWhatsNewDaysAfterFre(0);
  // Set the number of launches to 0.
  UpdateWhatsNewLaunchesAfterFre(0);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the What's New promo registers in the promo manager after 6 days
// have been recorded.
TEST_F(WhatsNewSceneAgentTest, TestWhatsNewPromoRegistrationWith6Days) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(promos_manager::Promo::WhatsNew))
      .Times(1);

  // Set the number of day after FRE to 6.
  UpdateWhatsNewDaysAfterFre(6);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the What's New promo did not register in the promo manager after 4
// days have been recorded.
TEST_F(WhatsNewSceneAgentTest, TestWhatsNewPromoNoRegistrationWith4Days) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(promos_manager::Promo::WhatsNew))
      .Times(0);

  // Set the number of day after FRE to 4.
  UpdateWhatsNewDaysAfterFre(4);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the What's New promo registers in the promo manager after 6
// launches have been recorded.
TEST_F(WhatsNewSceneAgentTest, TestWhatsNewPromoRegistrationWith6Launches) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(promos_manager::Promo::WhatsNew))
      .Times(1);

  // Set the number of launches to 6.
  UpdateWhatsNewLaunchesAfterFre(6);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the What's New promo did not register in the promo manager after 3
// launches have been recorded.
TEST_F(WhatsNewSceneAgentTest, TestWhatsNewPromoNoRegistrationWith3Launches) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(promos_manager::Promo::WhatsNew))
      .Times(0);

  // Set the number of launches to 3.
  UpdateWhatsNewLaunchesAfterFre(3);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

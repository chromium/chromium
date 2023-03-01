// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/promo/whats_new_scene_agent.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/browser_launcher.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/app/main_application_delegate.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/mock_promos_manager.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/main/test/fake_scene_state.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  [[NSUserDefaults standardUserDefaults] setObject:nil
                                            forKey:kWhatsNewDaysAfterFre];
  [[NSUserDefaults standardUserDefaults] setInteger:0
                                             forKey:kWhatsNewLaunchesAfterFre];
  [[NSUserDefaults standardUserDefaults] setBool:NO
                                          forKey:kWhatsNewPromoRegistrationKey];
  [[NSUserDefaults standardUserDefaults] setBool:NO
                                          forKey:kWhatsNewUsageEntryKey];
}

}  // namespace

class WhatsNewSceneAgentTest : public PlatformTest {
 public:
  WhatsNewSceneAgentTest() : PlatformTest() {
    std::unique_ptr<TestChromeBrowserState> browser_state_ =
        TestChromeBrowserState::Builder().Build();
    std::unique_ptr<Browser> browser_ =
        std::make_unique<TestBrowser>(browser_state_.get());
    id browser_launcher_mock_ =
        [OCMockObject mockForProtocol:@protocol(BrowserLauncher)];
    FakeStartupInformation* startup_information_ =
        [[FakeStartupInformation alloc] init];
    id main_application_delegate_ =
        [OCMockObject mockForClass:[MainApplicationDelegate class]];
    AppState* app_state =
        [[AppState alloc] initWithBrowserLauncher:browser_launcher_mock_
                               startupInformation:startup_information_
                              applicationDelegate:main_application_delegate_];
    promos_manager_ = std::make_unique<MockPromosManager>();
    agent_ = [[WhatsNewSceneAgent alloc]
        initWithPromosManager:promos_manager_.get()];
    scene_state_ =
        [[FakeSceneState alloc] initWithAppState:app_state
                                    browserState:browser_state_.get()];
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);
    agent_.sceneState = scene_state_;
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
  }

 protected:
  WhatsNewSceneAgent* agent_;
  FakeSceneState* scene_state_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the What's New promo did not register in the promo manager when
// the conditions aren't met.
TEST_F(WhatsNewSceneAgentTest, TestWhatsNewNoPromoRegistration) {
  ClearWhatsNewUserData();
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
  ClearWhatsNewUserData();
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
  ClearWhatsNewUserData();
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
  ClearWhatsNewUserData();

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
  ClearWhatsNewUserData();

  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(promos_manager::Promo::WhatsNew))
      .Times(0);

  // Set the number of launches to 3.
  UpdateWhatsNewLaunchesAfterFre(3);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

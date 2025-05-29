// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_promo_scene_agent.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/promos_manager/model/mock_promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class BWGPromoSceneAgentTest : public PlatformTest {
 public:
  BWGPromoSceneAgentTest() : PlatformTest() {
    scene_state_ = [[SceneState alloc] initWithAppState:app_state_];
    scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);
    std::unique_ptr<TestProfileIOS> profile_ =
        TestProfileIOS::Builder().Build();
    std::unique_ptr<Browser> browser_ =
        std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    FakeStartupInformation* startup_information_ =
        [[FakeStartupInformation alloc] init];
    app_state_ =
        [[AppState alloc] initWithStartupInformation:startup_information_];
    promos_manager_ = std::make_unique<MockPromosManager>();
    agent_ = [[BWGPromoSceneAgent alloc]
        initWithPromosManager:promos_manager_.get()];

    agent_.sceneState = scene_state_;
  }

 protected:
  BWGPromoSceneAgent* agent_;
  // SceneState only weakly holds AppState, so keep it alive here.
  AppState* app_state_;
  base::test::ScopedFeatureList feature_list_;
  SceneState* scene_state_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<MockPromosManager> promos_manager_;
};

// Tests that the BWG promo single registers in the promo manager.
TEST_F(BWGPromoSceneAgentTest, TestBWGPromoRegistration) {
  feature_list_.InitAndEnableFeature(kPageActionMenu);
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(promos_manager::Promo::BWGPromo))
      .Times(1);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the BWG promo is not registered in the promo
// manager when the eligibility criteria is not met.
TEST_F(BWGPromoSceneAgentTest, TestBWGPromoNoRegistration) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(promos_manager::Promo::BWGPromo))
      .Times(0);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

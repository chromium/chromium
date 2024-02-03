// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/omnibox_position/promo/omnibox_position_choice_scene_agent.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/browser/promos_manager/model/mock_promos_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/first_run_test_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"

namespace {

/// Number of days since first run to be considered as an existing user. This is
/// larger than the real value so it doesn't have to be updated when the real
/// value changes.
constexpr NSInteger kFirstRunRecencyForExistingUser = 100;

}  // namespace

class OmniboxPositionChoiceSceneAgentTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    feature_list_.InitAndEnableFeature(kBottomOmniboxPromoAppLaunch);

    FakeStartupInformation* startup_information_ =
        [[FakeStartupInformation alloc] init];
    app_state_ =
        [[AppState alloc] initWithStartupInformation:startup_information_];
    scene_state_ = [[SceneState alloc] initWithAppState:app_state_];
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);

    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), scene_state_);
    promos_manager_ = std::make_unique<MockPromosManager>();
    agent_ = [[OmniboxPositionChoiceSceneAgent alloc]
        initWithPromosManager:promos_manager_.get()
              forBrowserState:browser_state_.get()];
    agent_.sceneState = scene_state_;
  }

  void TearDown() override {
    PlatformTest::TearDown();
    // Clear first run sentinel and user default.
    ResetFirstRunSentinel();
  }

 protected:
  OmniboxPositionChoiceSceneAgent* agent_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  // SceneState only weakly holds AppState, so keep it alive here.
  AppState* app_state_;
  SceneState* scene_state_;
  base::test::TaskEnvironment task_environment_;
};

// Tests that the promo gets registered when the conditions are met.
TEST_F(OmniboxPositionChoiceSceneAgentTest, TestPromoRegistration) {
  // OmniboxPositionChoice is only available on phones.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return;
  }
  ForceFirstRunRecency(kFirstRunRecencyForExistingUser);

  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForContinuousDisplay(promos_manager::Promo::OmniboxPosition))
      .Times(1);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::OmniboxPosition))
      .Times(0);

  // Register the promo when there is no user preferred omnibox position.
  browser_state_->GetPrefs()->ClearPref(prefs::kBottomOmnibox);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the promo does not get registered when there is an existing
// preferred omnibox position.
TEST_F(OmniboxPositionChoiceSceneAgentTest,
       TestNoPromoRegistrationExistingPosition) {
  // OmniboxPositionChoice is only available on phones.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return;
  }
  ForceFirstRunRecency(kFirstRunRecencyForExistingUser);

  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForContinuousDisplay(promos_manager::Promo::OmniboxPosition))
      .Times(0);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::OmniboxPosition))
      .Times(1);

  // Deregister the promo if there is a user preferred omnibox position.
  browser_state_->GetPrefs()->SetBoolean(prefs::kBottomOmnibox, false);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the promo does not get registered for new users.
TEST_F(OmniboxPositionChoiceSceneAgentTest, TestNoPromoRegistrationNewUser) {
  // OmniboxPositionChoice is only available on phones.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return;
  }

  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForContinuousDisplay(promos_manager::Promo::OmniboxPosition))
      .Times(0);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::OmniboxPosition))
      .Times(1);

  // The promo should not register for new users.
  ResetFirstRunSentinel();
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the promo derigisters when a preferred omnibox position is set.
TEST_F(OmniboxPositionChoiceSceneAgentTest, TestDeregistration) {
  // OmniboxPositionChoice is only available on phones.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return;
  }
  ForceFirstRunRecency(kFirstRunRecencyForExistingUser);

  scene_state_.UIEnabled = YES;
  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForContinuousDisplay(promos_manager::Promo::OmniboxPosition))
      .Times(1);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::OmniboxPosition))
      .Times(0);

  // Register the promo on app foreground when there is no user preferred
  // omnibox position.
  browser_state_->GetPrefs()->ClearPref(prefs::kBottomOmnibox);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_CALL(
      *promos_manager_.get(),
      RegisterPromoForContinuousDisplay(promos_manager::Promo::OmniboxPosition))
      .Times(0);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::OmniboxPosition))
      .Times(1);

  // Deregister the promo if a preferred omnibox position is set.
  browser_state_->GetPrefs()->SetBoolean(prefs::kBottomOmnibox, false);

  scene_state_.UIEnabled = NO;
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/backend_promo/model/backend_promo_profile_agent.h"

#import <memory>

#import "base/functional/bind.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/backend_promo/model/backend_promo_service.h"
#import "ios/chrome/browser/backend_promo/model/backend_promo_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class FakeBackendPromoService : public BackendPromoService {
 public:
  FakeBackendPromoService() = default;
  ~FakeBackendPromoService() override = default;

  void NotifyBackendAppForegroundActive() override { notify_count_++; }

  int notify_count() const { return notify_count_; }

 private:
  int notify_count_ = 0;
};

std::unique_ptr<KeyedService> CreateFakeBackendPromoService(
    ProfileIOS* profile) {
  return std::make_unique<FakeBackendPromoService>();
}

}  // namespace

class BackendPromoProfileAgentTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        BackendPromoServiceFactory::GetInstance(),
        base::BindRepeating(&CreateFakeBackendPromoService));
    profile_ = std::move(builder).Build();

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    profile_state_.profile = profile_.get();

    agent_ = [[BackendPromoProfileAgent alloc] init];
    [profile_state_ addAgent:agent_];
  }

  void TearDown() override {
    [profile_state_ removeAgent:agent_];
    profile_state_.profile = nullptr;
    profile_state_ = nil;
    agent_ = nil;
    PlatformTest::TearDown();
  }

  FakeBackendPromoService* GetBackendPromoService() {
    return static_cast<FakeBackendPromoService*>(
        BackendPromoServiceFactory::GetForProfile(profile_.get()));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* profile_state_;
  BackendPromoProfileAgent* agent_;
};

// Tests that if a scene becomes foreground active before kFinal, the service
// is notified once the profile reaches kFinal init stage.
TEST_F(BackendPromoProfileAgentTest, NotifyWhenProfileReachesFinal) {
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kUIReady);

  SceneState* scene_state = [[SceneState alloc] init];
  scene_state.profileState = profile_state_;
  [profile_state_ sceneStateConnected:scene_state];

  // At kUIReady, scene entering foreground should not notify service yet.
  scene_state.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_EQ(0, GetBackendPromoService()->notify_count());

  // Transitioning to kFinal while scene is foreground active should notify
  // once.
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);
  EXPECT_EQ(1, GetBackendPromoService()->notify_count());
}

// Tests that scene activation changes trigger notifications when profile stage
// is kFinal.
TEST_F(BackendPromoProfileAgentTest, NotifyOnForegroundActiveTransitions) {
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);

  SceneState* scene_state = [[SceneState alloc] init];
  scene_state.profileState = profile_state_;
  [profile_state_ sceneStateConnected:scene_state];

  EXPECT_EQ(0, GetBackendPromoService()->notify_count());

  scene_state.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_EQ(1, GetBackendPromoService()->notify_count());

  scene_state.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(1, GetBackendPromoService()->notify_count());

  scene_state.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_EQ(2, GetBackendPromoService()->notify_count());
}

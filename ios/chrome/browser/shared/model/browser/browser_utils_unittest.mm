// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_utils.h"

#import <memory>

#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class BrowserUtilsTest : public PlatformTest {
 protected:
  BrowserUtilsTest() {
    profile_ = TestProfileIOS::Builder().Build();
    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
  }

  void TearDown() override {
    [scene_state_ shutdown];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeSceneState* scene_state_;
};

// Tests that the regular Browser for a scene is a regular Browser of that
// scene, even while the scene's incognito Browser is current.
TEST_F(BrowserUtilsTest, RegularBrowserForSceneIsRegularBrowserOfScene) {
  [scene_state_
      setCurrentBrowser:scene_state_.browserProviderInterface.incognitoBrowser];

  Browser* browser = GetRegularBrowserForScene(scene_state_);

  ASSERT_NE(browser, nullptr);
  EXPECT_EQ(browser->type(), Browser::Type::kRegular);
  EXPECT_EQ(browser->GetSceneState(), scene_state_);
}

}  // namespace

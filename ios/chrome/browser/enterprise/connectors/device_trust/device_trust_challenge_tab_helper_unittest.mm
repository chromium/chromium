// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/device_trust_challenge_tab_helper.h"

#import <memory>

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class DeviceTrustChallengeTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    DeviceTrustChallengeTabHelper::CreateForWebState(web_state_.get());
  }

  DeviceTrustChallengeTabHelper* helper() {
    return DeviceTrustChallengeTabHelper::FromWebState(web_state_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

TEST_F(DeviceTrustChallengeTabHelperTest, CreatesSuccessfully) {
  EXPECT_NE(helper(), nullptr);
}

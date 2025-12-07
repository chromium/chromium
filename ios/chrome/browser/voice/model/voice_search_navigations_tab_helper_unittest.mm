// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/model/voice_search_navigations_tab_helper.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for VoiceSearchNavigations.
class VoiceSearchNavigationsTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);

    VoiceSearchNavigationTabHelper::CreateForWebState(web_state());
  }

  VoiceSearchNavigationTabHelper* navigations() {
    return VoiceSearchNavigationTabHelper::FromWebState(web_state());
  }

 protected:
  web::WebState* web_state() { return web_state_.get(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
};

// Tests that a navigation commit reset the value of
// IsExpectingVoiceSearch().
TEST_F(VoiceSearchNavigationsTest, CommitResetVoiceSearchExpectation) {
  navigations()->WillLoadVoiceSearchResult();
  EXPECT_TRUE(navigations()->IsExpectingVoiceSearch());
  web::test::LoadHtml(@"<html></html>", web_state());
  EXPECT_FALSE(navigations()->IsExpectingVoiceSearch());
}

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/voice_search_navigations_tab_helper.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for VoiceSearchNavigations.
class VoiceSearchNavigationsTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);

    VoiceSearchNavigationTabHelper::CreateForWebState(web_state());
  }

  VoiceSearchNavigationTabHelper* navigations() {
    return VoiceSearchNavigationTabHelper::FromWebState(web_state());
  }

 protected:
  web::WebState* web_state() { return web_state_.get(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
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
